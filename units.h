/*	Adress book for units (daemons and client) for IPC intercommunications
	Each unit connected via channel (id for message queue; shm for data exchange)
*/

#include <glib.h>

typedef struct channel_info_t {
	int queue_id;			// address id for message_queue
	int online;
	char* shm_name;		// shared memory path
	char* sem_name;		// semaphore path
} channel_info_t;

/*	register unit by id and create channel (pair message queue and shared memory) with requested size
	returns: 0 - all ok
	-1 - invalid arguments
	-2 - channel allocation failure
*/

int register_unit (const char* id, unsigned int requested_packet_size, channel_info_t* info);
void unregister_unit (const char* id);

const GData* get_units ();
void destroy_unit_list ();
