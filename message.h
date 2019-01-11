/* Basic message declarations. Messages can be chained similar to lists in linux kernel.
*/

typedef struct bifrost_address_t
{
	int ip;		// 0 if local; otherwise - LAN ip
	int id;		// local id
} bifrost_address_t;


typedef enum {
	MESSAGE_DATA,		// routed data
	MESSAGE_COMMAND		// command to bifrost
} message_type_t;

// base message
typedef struct message_t {
	message_type_t message_type;
	unsigned int message_size;
	struct message_t* next;
} message_t;


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

//=================================================================================================

// data message
typedef struct data_message_t {
	// common header
	message_type_t message_type;
	unsigned int message_size;
	message_t*	next;

	bifrost_address_t src_id;
	bifrost_address_t dest_id;
	unsigned int buffer_size;
	char 	buf[0];		// actually, this buffer will be buffer_size length
} data_message_t;

//----------------------------------------------------------------------------------------------------

// command message
typedef enum command_type_t {
	BIFROST_CONNECT = 1,
	BIFROST_DISCONNECT,
	BIFROST_SET_MESSAGE_BATCH_SIZE,
	BIFROST_REGISTER_UNIT,
	BIFROST_REGISTER_REMOTE_UNIT,
	BIFROST_UNREGISTER_UNIT
} command_type_t;

typedef struct command_t {
	// common header
	message_type_t message_type;
	unsigned int message_size;
	message_t*	next;

	command_type_t command_type;
	unsigned int buffer_size;
	char	args[0];	// arguments buffer
} command_t;

//====================================================================================================
// command structures

typedef struct bifrost_register_unit_command_t {
	int packet_size;	// shared memory size request
	char name[0];		// unit name
} bifrost_register_remote_unit_command_t;

typedef struct bifrost_register_remote_unit_command_t {
	int ip;
	int id;
	char name[0];
} bifrost_register_unit_command_t;


