#include "units.h"
#include <string.h>
#include <syslog.h>
#include "consts.h"
//#include "ipc.h"

GData* unit_list = NULL;

void free_channel_info (gpointer data)
{
	free (((channel_info_t*)data)->shm_name);
	free (((channel_info_t*)data)->sem_name);
	free ((channel_info_t*)data);
}

// it's quite safe to use this - if some daemon will constantly respawn, it will have same d-bus-id.
int last_id = BIFROST_DAEMON_QUEUE_ID; // core daemon id is always first

int register_unit (const char* id, unsigned int requested_packet_size, channel_info_t* info)
{
	channel_info_t* element = NULL;
	GQuark	 key;

	if (!id || !info)
	{
		syslog (LOG_ERR, "Invalid arguments: no id or info!");
		return -1;
	}

#pragma message "unimplemented channel allocation"
	syslog (LOG_DEBUG, "requested register (id=%s, size=%u)", id, requested_packet_size);

	if (!unit_list)
	{
		syslog (LOG_DEBUG, "allocating unit datalist");
		g_datalist_init(&unit_list);
	}

	key = g_quark_from_string(id);
	element = (channel_info_t*)g_datalist_id_get_data (&unit_list, key);
	if (!element)
	{
		syslog (LOG_DEBUG, "new element => allocating memory");
		element = (channel_info_t*)malloc (sizeof (channel_info_t));
		memset (element, 0, sizeof (channel_info_t));
		if (!element)
		{
			syslog (LOG_DEBUG, "calloc failed!");
			return -2;
		}

		element->queue_id = ++last_id;
		element->online = 1;
		element->shm_name = requested_packet_size > 0 ? strdup("/some/test/shm") : NULL;
		element->sem_name = requested_packet_size > 0 ? strdup("/some/test/sem") : NULL;

		if ((requested_packet_size > 0) && !element->shm_name)
		{
			syslog (LOG_DEBUG, "shared memory allocation failed!");
			return -2;
		}

		g_datalist_id_set_data_full (&unit_list, key, element, free_channel_info);
	}

	*info = *element;
//	info->queue_id = element->queue_id;
//	info->shm_name = element->shm_name;
//	info->sem_name = element->sem_name;
	syslog (LOG_DEBUG, "element in list[%s]: {%d, %s}", id, info->queue_id, info->shm_name);

	return 0;
}

void unregister_unit (const char* id)
{
//	g_dataset_id_remove_data (&unit_list, g_quark_from_string(id));
	channel_info_t* element = (channel_info_t*)g_datalist_id_get_data (&unit_list,  g_quark_from_string(id));
	if (element)
		element->online = 0;
	syslog (LOG_DEBUG, "%s marked as offline", id);
}

void destroy_unit_list ()
{
	g_object_unref (unit_list);
	unit_list = NULL;
	syslog (LOG_DEBUG, "datalist destroyed");
}

const GData* get_units ()
{
	return unit_list;
}
