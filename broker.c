#include "broker.h"
#include "message.h"
#include "settings.h"
#include "ipc/ipc.h"
#include <strings.h>
#include <syslog.h>

enum {
	BIFROST_DAEMON_QUEUE_ID = 1	//reserved for core daemon
};

/* address book
	holds unit addresses available in system
	unit address - [<ip>:<number>]
*/

typedef struct bifrost_address_record_t {
	char* name;
	bifrost_address_t address;
} bifrost_address_record_t;

/* local channel description
	keyed by bifrost_id from address book
*/
typedef struct channel_info_t {
//	int queue_id;			// address id for message_queue
	int online;
	char* shm_name;			// shared memory path
	char* sem_name;			// semaphore path
	channel_t* channel;
} channel_info_t;

static GSList* address_book = NULL;
static GArray* channels = NULL;	// index = bifrost_id - 2, because ids 0 and 1 are reserved

#define BIFROST_ID_TO_CHANNEL_INDEX(id) id - 2
#define CHANNEL_INDEX_TO_BIFROST_ID(idx) idx + 2

bifrost_address_record_t* find_address (const char* name)
{
	GSList* item == NULL;

	for (item = address_book; item; item = g_slist_next(item))
	{
		if (g_strcmp0(((bifrost_address_record_t*)item->data)->name, name) == 0)
			break;
	}

	return (item != NULL) ? item->data : NULL;
}

//====================================================================================================================
// local unit registration
int register_unit (const char* name, unsigned int requested_packet_size)
{
	bifrost_address_record_t* record = NULL;
	channel_info_t channel = NULL;
	int bifrost_id = -1;
	const char* prefix = bifrost_settings.channel_prefix;

	if (!name)
	{
		syslog (LOG_ERR, "Invalid arguments: no id or info!");
		return -1;
	}

	syslog (LOG_DEBUG, "requested register (id=%s, size=%u)", id, requested_packet_size);

	record = find_address (name);

	if (!record)
	{
		syslog (LOG_DEBUG, "new element => allocating memory");
		record = malloc (sizeof (bifrost_address_record_t));
		memset (record, 0, sizeof (bifrost_address_record_t));
		if (!record)
		{
			syslog (LOG_DEBUG, "malloc failed!");
			return -2;
		}
		
		channel.online = 1;
		channel.shm_name = NULL;
		channel.sem_name = NULL;
		channel.channel = NULL;
		if (requested_packet_size > 0)
		{
			int len = strlen(prefix) + strlen(name) + strlen("_shm") + 1;
			channel.shm_name = malloc (len);
			memset (channel.shm_name, 0, len);
			channel.shm_name = strcat(channel.shm_name, prefix);
			channel.shm_name = strcat(channel.shm_name, name);
			channel.shm_name = strcat(channel.shm_name, "_shm");

			len = strlen(prefix) + strlen(name) + strlen("_sem") + 1;
			channel.sem_name = malloc (len);
			memset (channel.sem_name, 0, len);
			channel.sem_name = strcat(channel.sem_name, prefix);
			channel.sem_name = strcat(channel.sem_name, name);
			channel.sem_name = strcat(channel.sem_name, "_sem");

			channel.channel = channel_open (channel.shm_name, channel.sem_name, requested_packet_size, TRUE);
		}

		// register new channel
		if (!channels)
		{
			channels = g_array_new (FALSE,				//zero-terminated
						TRUE,				//memset new val to 0
						sizeof(channel_info_t));
		}

		record->name = strdup (name);
		record->address.ip = 0;	// local address
		record->address.id = CHANNEL_INDEX_TO_BIFROST_ID(channels->len);

		g_array_append_val (channels, channel);
		address_book = g_slist_append(address_book, record);

		syslog (LOG_DEBUG, "added records:\n\tto addressbook: [%s]:{%i, %i}"
				   "\n\tto channels list: {%s, %s}", name, address->ip, address->id,
									   channel.shm_name, channel.sem_name);

		bifrost_dbus_emit_signal (BIFROST_SIGNAL_CHANNEL_REGISTERED, name, address->id, channel.shm_name, channel.sem_name);
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
// remote unit registration
int register_unit (const char* name, int ip, int id)
{
	bifrost_address_record_t* record = NULL;
	GQuark	 key;

	if (!name)
	{
		syslog (LOG_ERR, "Invalid arguments: no id or info!");
		return -1;
	}

	syslog (LOG_DEBUG, "requested register (id=%s, size=%u)", id, requested_packet_size);

	record = find_address (name);

	if (!record)
	{
		syslog (LOG_DEBUG, "new element => allocating memory");
		record = malloc (sizeof (bifrost_address_record_t));
		memset (record, 0, sizeof (bifrost_address_record_t));
		if (!record)
		{
			syslog (LOG_DEBUG, "malloc failed!");
			return -2;
		}
		
		record->name = strdup (name);
		record->ip = ip;
		record->id = id;

		address_book = g_slist_append(address_book, record);
		syslog (LOG_DEBUG, "added records:\n\tto addressbook: [%s]:{%i, %i}", name, address->ip, address->id);
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------

void unregister_unit (const char* name)
{
	bifrost_address_t* address = NULL;
	channel_info_t* channel = NULL;
	if (!address_book)	// nothing to do
		return;
	
	address = g_datalist_id_get_data (&address_book,  g_quark_from_string(name));

	if (address && (address->ip == 0))
	{
		if (BIFROST_ID_TO_CHANNEL_INDEX(address->id) < channels->len)
		{
			channel = &g_array_index(channels, channel_info_t, BIFROST_ID_TO_CHANNEL_INDEX(address->id));
			channel->online = 0;
			channel_close (channel->channel);
			syslog (LOG_INFO, "unit [%s]:{%i:%i} is marked offline", name, address->ip, address->id);
		}
	} else
	{
		g_dataset_id_remove_data (&unit_list, g_quark_from_string(id));
		syslog (LOG_INFO, "unit [%s]:{%i:%i} is removed", name, address->ip, address->id);
	}
}

//=================================================================================================



//=================================================================================================

static unsigned int message_batch_count = bifrost_settings.message_batch_size;

void route_message (data_message_t* msg);
void execute_message (command_t* msg);

//-------------------------------------------------------------------------------------------------
// main broker function
void process_bus_messages ()
{
	message_t* message = NULL;
	unsigned int i;

	for (i = 0; i < message_batch_count; i++)
	{
		message = bifrost_pop_message ();
		if (message->message_type == MESSAGE_DATA)
		{
			route_message (message);
		} else if (message->message_type == MESSAGE_COMMAND)
		{
			execute (message);
		}
	}
}

//-------------------------------------------------------------------------------------------------

void execute_message (command_t* msg)
{
	switch (msg->command_type)
	{
	case BIFROST_CONNECT:
		syslog (LOG_WARNING, "BIFROST_CONNECT is not implemented yet");
		break;

	case BIFROST_DISCONNECT:
		syslog (LOG_WARNING, "BIFROST_DISCONNECT is not implemented yet");
		break;

	case BIFROST_SET_MESSAGE_BATCH_SIZE:
		if (msg->buffer_size == sizeof(unsigned int))
		{
			message_batch_count = *(unsigned int*)(msg->args);
			syslog (LOG_INFO, "batch size changed to %u", message_batch_count);
		} else 
			syslog (LOG_ERROR, "incorrect arguments buffer size: (should be %u, got %u)", sizeof(unsigned int), msg->buffer_size);
		break;

	case BIFROST_REGISTER_UNIT:
		if (msg->buffer_size >= sizeof(bifrost_register_unit_command_t))
		{
			bifrost_register_unit_command_t* cmd = (bifrost_register_unit_command_t*) msg->args;
			register_unit (cmd->name, cmd->packet_size);
		}
		break;

	case BIFROST_REGISTER_REMOTE_UNIT:
		if (msg->buffer_size >= sizeof(bifrost_register_remote_unit_command_t))
		{
			bifrost_register_remote_unit_command_t* cmd = (bifrost_register_remote_unit_command_t*) msg->args;
			register_unit (cmd->name, cmd->ip, cmd->id);
		}
		break;

	case BIFROST_UNREGISTER_UNIT:
		if (msg->buffer_size > 0)
			unregister_unit (cmd->buf);
		break;

	default:
		syslog (LOG_WARNING, "Unimplemented command type %i", msg->command_type);
	}
}

//-------------------------------------------------------------------------------------------------

void route_message (data_message_t* msg)
{
#pragma message "not implemented!"
}

//-------------------------------------------------------------------------------------------------
void address_book_record_free(bifrost_address_book_record_t* rec)
{
	free (rec->name);
	free (rec);
}

void broker_uninit ()
{
	int idx;

	if (channels) {
		// close all channels and remove them
		for (idx = 0; idx < channels->len; idx++)
		{
			channel_info_t* ch = &g_array_index (channels, channel_info_t, idx);
			channel_close (ch->channel);
			free (ch->shm_name);
			free (ch->sem_name);
		}
		g_array_free (channels);
		channels = NULL;
	}

	// remove addresses
	if (address_book) {
		g_slist_free_full (address_book, address_book_record_free);
		address_book = NULL;
	}
}

