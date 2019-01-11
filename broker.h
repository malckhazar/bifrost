// Message broker - receives message from bus and routes or executes it

// address book

// local unit
//int register_unit (const char* name, unsigned int requested_packet_size);
// remote unit (from avahi-browse)
//int register_unit (const char* name, int ip, int id);

/* local units are only marked as offline (needed for id reacquisition)
   remote units are removed
*/ 
//void unregister_unit (const char* name);

// main functions
void process_bus_messages ();
void broker_uninit ();
