#include "ipc.h"
#include "../settings.h"
#include "../units.h"
// message queue
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>


#pragma message "Will need to update MAX_SEND_SIZE later"
#pragma message "unimplemented shared memory"

int queue_id = -1;
const char ftok_app_id = 'm';

#define MAX_SEND_SIZE 80

struct {
	long type;
	unsigned int size;
	char data[MAX_SEND_SIZE];
} buffer;

int  queue_create()
{
	key_t key;
	key = ftok (settings.queue_path, ftok_app_id);

	if ((queue_id = msgget(key, IPC_CREAT | 0660)) == -1)
	{
		syslog (LOG_CRIT, "Failed to create IPC message queue!");
		return -1;
	}
	return 0;
}

int queue_send_message(long type, char *text, unsigned int size)
{
	if (queue_id < 0)
		return -3;

	if (size > MAX_SEND_SIZE)
	{
		syslog (LOG_ERR, "message size is too big: size=%i max=%i", size, MAX_SEND_SIZE);
		return -1;
	}

	syslog (LOG_DEBUG, "sending message to %i", type);
	buffer.type = type;
 	buffer.size = size;
	memcpy (buffer.data, text, size);

	if ((msgsnd (queue_id, (struct msgbuf *)&buffer, size + sizeof (unsigned int), 0)) == -1)
	{
		syslog (LOG_ERR, "message queue send msg returned: %s", strerror(errno));
		return -2;
	}
	return 0;
}

int queue_receive_message(long type, char**text, unsigned int* buffersize)
{
	if (queue_id < 0)
		return -2;

	if (!text || !buffersize)
	{
		syslog (LOG_ERR, "queue_receive_message: invalid arguments!");
		return -1;
	}

	syslog (LOG_DEBUG, "receiving message to %i", type);
	buffer.type = type;
	msgrcv (queue_id, (struct msgbuf *)&buffer, MAX_SEND_SIZE + sizeof (unsigned int), type, 0);

	if (buffer.size == 0)	// there is no message
	{
		syslog (LOG_INFO, "received empty message");
		return 0;
	}

	if (buffersize < buffer.size || !*text)
	{
		// reallocate buffer
		*text = (char*) realloc (*text, buffer.size);
		buffersize = buffer.size;
	}
	memcpy (*text, buffer.data, buffer.size);
	return buffer.size;
}

void queue_destroy()
{
	if (queue_id < 0)	// nothing to do
		return;

	msgctl (queue_id, IPC_RMID, 0);
	queue_id = -1;
}

struct callback_info_t {
	char *text;
	unsigned int size;
};

static void
foreach_callback (GQuark key_id,
		gpointer data,
		gpointer user_data)
{
	const channel_info_t* ch = (const channel_info_t*) data;
	unsigned int size = (unsigned int)user_data;
	
	if (ch->online == 0)
		return;

	buffer.type = ch->queue_id;
	if ((msgsnd (queue_id, (struct msgbuf *)&buffer, size, 0)) == -1)
	{
		syslog (LOG_ERR, "message queue send msg returned: %s", strerror(errno));
	}
}

void queue_broadcast_message (char *text, unsigned int size)
{
	GData* units = NULL;
	struct callback_info_t cbdata;
	if (queue_id < 0)
		return;

	if (size + sizeof(unsigned int) > MAX_SEND_SIZE)
	{
		syslog (LOG_ERR, "queue_broadcast_message: message size is too big: size=%i max=%i", size, MAX_SEND_SIZE);
		return;
	}

	units = get_units();

	syslog (LOG_DEBUG, "broadcasting messages...");
 	*(unsigned int*)buffer.data = size;
	memcpy (buffer.data + sizeof(unsigned int), text, size);

	g_datalist_foreach (&units, foreach_callback, size + sizeof(unsigned int));
}

//=======================================================================================
/*	Channel is ipc composed from shared memory and semaphore, guarding it
*/

typedef struct channel_t
{
	int shm;	// shared memory descriptor
	char* segment;  // shared memory segment pointer
	int size;	// segment size (without reserved)
	int sem;	// semaphore descriptor
	int owner;	// semaphore ownership flag
} channel_t;

channel_t* channel_open (char* shm_path, char* sem_path, int required_size, int owner)
{
	channel_t* chan = NULL;
	key_t key;
	void* seg = NULL;

	// in some linux systems this union is undefined due POSIX.1 requirements
#ifdef _SEM_SEMUN_UNDEFINED
	union semun {
		int val;                /* value for SETVAL */
		struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
		ushort *array;          /* array for GETALL & SETALL */
		struct seminfo *__buf;  /* buffer for IPC_INFO */
		void *__pad;
        };		
#endif

	union semun semopts;

	syslog (LOG_DEBUG, "creating a channel ('%s', '%s')...", shm_path, sem_path);

	if (shm_path == NULL || strlen(shm_path) == 0
		 || sem_path == NULL || strlen(sem_path) == 0
		 || required_size <= 0)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return NULL;
	}

	chan = (channel_t*) malloc (sizeof(channel_t));
	chan->shm = -1;
	chan->segment = NULL;
	chan->sem = -1;
	chan->owner = owner;

	// create shared memory object
	key = ftok(shm_path, ftok_app_id);
	if ((chan->shm = shmget(key, required_size + sizeof(unsigned int), IPC_CREAT | 0660)) == -1)
	{
		syslog (LOG_ERR, "%s: failed to create shm object at ''!", __func__, shm_path);
		free (chan);
		return NULL;
	}

	// map region to process (region will be aligned to page size)
	if ((seg = shmat(chan->shm, 0, 0)) == (void*)-1)
	{
		syslog (LOG_ERR, "%s: failed to attach shm object to process", __func__);
		shmctl (chan->shm, IPC_RMID, 0);	// mark for deletion
		free (chan);
		return NULL;
	} else chan->segment = seg;
	chan->size = required_size;

	// create shared semaphore
	key = ftok(sem_path, ftok_app_id);
	// semget allocates a set of semaphores.
	// Unfortunately, there is no way to resize this set, so there will be many sets :(
	if ((chan->sem = semget(key, 1, IPC_CREAT | 0660)) == -1)
	{
		syslog (LOG_ERR, "%s: failed to create semaphore at '%s'!", __func__, sem_path);
		shmdt (chan->segment);
		shmctl (chan->shm, IPC_RMID, 0);	// mark for deletion
		free (chan);
	}
	// sem initialization
	semopts.val = 1; // initial value and maximum of resource
	semctl (chan->sem, 0, SETVAL, semopts);

	*(unsigned int*)(chan->segment) = 0;	// data size

	syslog (LOG_DEBUG, "channel ('%s', '%s') created", shm_path, sem_path);
	return chan;
}

void channel_close 	(channel_t* chan)
{
	if (!chan) return; // nothing to do

	shmdt (chan->segment);	//detach shared segment
	shmctl (chan->shm, IPC_RMID, 0);	// mark shared object for deletion
	if (chan->owner)
		semctl (chan->sem, IPC_RMID, 0); // remove sem from kernel - only owner must do that
	free (chan);
}

// semaphore operations
struct sembuf sem_lock={ 0, -1, 0 };
struct sembuf sem_unlock={ 0, 1, IPC_NOWAIT };

int channel_write	(channel_t* chan, const char* buffer, unsigned int size)
{
	// checks
	if (!chan || !buffer || size == 0)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}
	
	if (size > chan->size)
	{
		syslog (LOG_ERR, "%s: attempted to write more than allocated!", __func__);
		return -2;
	}

	// sem lock
	if (semop(chan->sem, &sem_lock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem lock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	// write into shm
	memcpy (chan->segment + sizeof (unsigned int), buffer, size);
	*(unsigned int*)(chan->segment) = size;

	// sem unlock
	if (semop(chan->sem, &sem_unlock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem unlock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	return size;
}

int channel_read 	(channel_t* chan, char** buffer, unsigned int* size)
{
	unsigned int datasize = 0;
	// checks
	if (!chan || !buffer || !size)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}
	
	datasize = *(unsigned int*) (chan->segment);

	if (datasize == 0)
		return 0;

	if (datasize > *size || !*buffer)
	{
		*buffer = (char*)realloc(*buffer, datasize);
		*size = datasize;
	}

	// sem lock
	if (semop(chan->sem, &sem_lock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem lock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	// read from shm
	memcpy (*buffer, chan->segment + sizeof (unsigned int), datasize);

	// sem unlock
	if (semop(chan->sem, &sem_unlock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem unlock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	return datasize;
}

// obtain/release lock
int channel_lock	(struct channel_t* chan)
{
	if (!chan)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}

	// sem lock
	if (semop(chan->sem, &sem_lock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem lock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	return 0;
}

int channel_unlock	(struct channel_t* chan)
{
	if (!chan)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}

	// sem lock
	if (semop(chan->sem, &sem_unlock, 1) == -1)
	{
		syslog (LOG_ERR, "%s: sem lock operation fault: %s", __func__, strerror(errno));
		return -3;
	}

	return 0;
}

// get pointer to data block
char* channel_get_dataptr (struct channel_t* chan)
{
	if (!chan)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return NULL;
	}

	return chan->segment ? chan->segment + sizeof(unsigned int) : NULL;
}

unsigned int channel_get_capacity (struct channel_t* chan)
{
	if (!chan)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}
	return chan->size;
}

unsigned int channel_get_data_size (struct channel_t* chan)
{
	if (!chan)
	{
		syslog (LOG_ERR, "%s: invalid arguments!", __func__);
		return -1;
	}
	return *(unsigned int*)(chan->segment);
}

void  channel_set_data_size (struct channel_t* chan, unsigned int size)
{
	if (!chan) syslog (LOG_ERR, "%s: invalid arguments!", __func__);
	else *(unsigned int*)(chan->segment) = size;
}

