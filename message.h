/* Basic message declarations. Messages can be chained similar to lists in linux kernel.
*/

typedef enum {
	MESSAGE_DATA,		// routed data
	MESSAGE_COMMAND,	// command to bifrost
	MESSAGE_EVENT		// event from d-bus -- currently disabled
} message_type_t;

// base message
typedef struct message_t {
	message_type_t message_type;
	unsigned int message_size;
	struct message_t* next;
} message_t;

/* I'm not sure, do I really need this?
// event
typedef enum event_type_t {
	EVENT_APPEARED,
	EVENT_DISAPPEARED
} event_type_t;

typedef struct message_event_t {
	// common header
	message_type_t 	message_type;
	unsigned int 	size;
	message_t*	next;

	event_type_t event_type;
	char* name;
} message_event_t;
*/


/* create message of desired type
	datasize is required size for a message buffer and for command arguments buffer.
	Others just ignore it.
*/
message_t* bifrost_create_message (message_type_t type, unsigned int datasize);
/* push message to bus */
void bifrost_push_message (message_t* msg);
/* pop message from bus */
message_t* bifrost_pop_message ();
/* clear bus */
void bifrost_clear_bus ();

//----------------------------------------------------------------------------------------------------

// data message
typedef struct data_message_t {
	// common header
	message_type_t message_type;
	unsigned int message_size;
	message_t*	next;

	int	src_id;
	int	dest_id;
	unsigned int buffer_size;
	char 	buf[1];		// actually, this buffer will be buffer_size length
} data_message_t;

//----------------------------------------------------------------------------------------------------

// command
typedef enum command_type_t {
	BIFROST_CONNECT,
	BIFROST_DISCONNECT,
	BIFROST_SET_MESSAGE_BATCH_SIZE
} command_type_t;

typedef struct command_t {
	// common header
	message_type_t message_type;
	unsigned int message_size;
	message_t*	next;

	command_type_t command_type;
	unsigned int buffer_size;
	char	args[1];	// arguments buffer
} command_t;

