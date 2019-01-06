#include "broker.h"
#include <syslog.h>

unsigned int message_batch_count = 5;

//=================================================================================================

void route_message (data_message_t* msg);
void execute_message (command_t* msg);

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

void execute_message (command_t* msg)
{
	if (msg->command_type == BIFROST_CONNECT)
	{
		syslog (LOG_WARNING, "BIFROST_CONNECT is not implemented yet");
	} else if (msg->command_type == BIFROST_DISCONNECT)
	{
		syslog (LOG_WARNING, "BIFROST_DISCONNECT is not implemented yet");
	} else if (msg->command_type == BIFROST_SET_MESSAGE_BATCH_SIZE)
	{
		if (msg->buffer_size == sizeof(unsigned int))
		{
			message_batch_count = *(unsigned int*)(msg->args);
			syslog (LOG_INFO, "batch size changed to %u", message_batch_count);
		} else 
			syslog (LOG_ERROR, "incorrect arguments buffer size: (should be %u, got %u)", sizeof(unsigned int), msg->buffer_size);
	}
}

void route_message (data_message_t* msg)
{
#pragma message "not implemented!"
}

