#include "message.h"
#include <pthread.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>

/* bus is a simple fifo */

message_t* head = NULL;
message_t* tail = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

message_t* bifrost_create_message (message_type_t type, unsigned int datasize)
{
	message_t* msg = NULL;
	int sz = 0;

	if (type == MESSAGE_DATA)		sz = sizeof (data_message_t) - sizeof (char) + datasize;
	else if (type == MESSAGE_COMMAND)	sz = sizeof (command_t) - sizeof (char) + datasize;
//	else if (type == MESSAGE_EVENT)		sz = sizeof (event_t);
	else {
		syslog (LOG_ERR, "%s unimplemented message type requested", __func__);
		return NULL;
	}

	msg = malloc (sz);
	memset (msg, 0, sz);
	
	msg->message_type = type;
	msg->message_size = sz;
	if (type == MESSAGE_DATA || type == MESSAGE_COMMAND)
		((data_message_t*)msg)->buffer_size = datasize;

	return msg;
}

void bifrost_push_message (message_t* msg)
{
	if (!msg) return;	// nothing to do

	pthread_mutex_lock (&mutex);	
	if (head == NULL)
	{
		head = msg;
		tail = msg;
	} else
	{
		tail->next = msg;
		tail = msg;
	}
	pthread_mutex_unlock (&mutex);
}

message_t* bifrost_pop_message ()
{
	message_t* msg = NULL;

	pthread_mutex_lock (&mutex);	
	if (head != NULL)
	{
		msg = head;
		head = head->next;
		if (head == NULL)
			tail = NULL;
	}
	pthread_mutex_unlock (&mutex);

	return msg;
}

void bifrost_clear_bus ()
{
	message_t* msg;

	pthread_mutex_lock (&mutex);

	for (; head; head = msg) {
		msg = head->next;
		free (head);
	}

	tail = NULL;

	pthread_mutex_unlock (&mutex);
}

