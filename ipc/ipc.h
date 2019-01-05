/* low-level IPC wrappers. I prefer to use SysV IPC functions here
*/

// message queue

/* creates queue
   returns: 0 = all ok, -1 = failure
*/
int  queue_create();

/* sends message
	input: id, buffer, data size
	returns: 0 - all ok
		 -1 - message is too large
		 -2 - send fail
		 -3 - no queue
*/
int  queue_send_message(long type, char *text, unsigned int size);

/* receive message
	input: id, pointer to buffer, pointer to buffersize
	if buffer is lesser that required, it will be reallocated! Caller is responsible for the memory
	returns: bytes read
		-1 -- invalid arguments
		-2 -- no queue
*/
int  queue_receive_message(long type, char**text, unsigned int* buffersize);
void queue_destroy();

void queue_broadcast_message (char *text, unsigned int size);

// channel - shared memory + semaphore

struct channel_t;

/* create or open an existing channel. Requires two paths of shared objects (shm, sem) and size of shared block
	owner flag determines who will release a sem object from kernel (it cannot be marked for deletion as shm for some reason)
	allocated shared segment size will always be required_size + sizeof(unsigned int).
	sizeof(unsigned int) is reserved for a size of data written into segment
*/
struct channel_t* channel_open (char* shm_path, char* sem_path, int required_size, int owner);
void channel_close 	(struct channel_t* channel);

// simple I/O operations
int channel_read 	(struct channel_t* channel, char** buffer, unsigned int* size);
int channel_write	(struct channel_t* channel, const char* buffer, unsigned int size);

// if someone will need to perform low-level ops...

// obtain/release lock
int channel_lock	(struct channel_t* channel);
int channel_unlock	(struct channel_t* channel);

// get pointer to data block
char* channel_get_dataptr (struct channel_t* channel);
unsigned int channel_get_capacity (struct channel_t* channel);
unsigned int channel_get_data_size (struct channel_t* channel);
void  channel_set_data_size (struct channel_t* channel, unsigned int size);

