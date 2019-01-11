#include "message.h"
#include "syslog.h"

//=================================================================================================

/*
void main_loop ()
{
	receive_from_network ();
	read_channels ();
	process_bus_messages ();
}
*/

int main (int argc, char**argv)
{
	message_t* msg;

	openlog("libdn-ipc", LOG_CONS|LOG_PERROR, LOG_USER);	
//	setlogmask (LOG_UPTO(LOG_DEBUG));

	msg = bifrost_create_message (MESSAGE_COMMAND, 0);
	bifrost_push_message (msg);

	msg = bifrost_create_message (MESSAGE_DATA, 10);
	bifrost_push_message (msg);

	syslog (LOG_INFO, "next error message is test, not an error");
	msg = bifrost_create_message (MESSAGE_EVENT, 0);

	bifrost_clear_bus ();

/*
	broker_uninit();
*/

	closelog ();
	return 0;
}
