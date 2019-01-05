#include "dbus.h"
#include <pthread.h>
#include <dbus/dbus.h>
#include <syslog.h>
#include <glib.h>
#include <gio/gio.h>

#include "../units.h"
#include "../settings.h"

const char *version = "0.1";

// D-Bus object interface description with all methods and properties
const char *server_introspection_xml =
	"<node>"
	"  <interface name='"DBUS_INTERFACE"'>"
	"    <annotation name='org.gtk.GDBus.Annotation' value='OnInterface'/>"
	"    <annotation name='org.gtk.GDBus.Annotation' value='AlsoOnInterface'/>"
	/* unit registration method
		in - daemon id, packet size
		out - names of message queue and shared memory
	*/
	"    <method name='RegisterUnit'>"
	"      <annotation name='org.gtk.GDBus.Annotation' value='ChannelRequest'/>"
	"      <arg type='s' name='id' direction='in'/>"
	"      <arg type='u' name='packetSize' direction='in'/>"
	"      <arg type='i' name='messgeQueueId' direction='out'/>"
	"      <arg type='s' name='shmPath' direction='out'/>"
	"      <arg type='s' name='semPath' direction='out'/>"
	"    </method>"
	/* unit requests to free allocated channel
	*/
	"    <method name='UnregisterUnit'>"
	"      <annotation name='org.gtk.GDBus.Annotation' value='FreeResources'/>"
	"      <arg type='s' name='id' direction='in'/>"
	"    </method>"
	/* signal emitted when core daemon is going to shutdown -- all daemons MUST disconnect from their mq&shm!
	*/
	"    <signal name='Shutdown'>"
	"      <annotation name='org.gtk.GDBus.Annotation' value='Onsignal'/>"
	"    </signal>"
	// version property
	"    <property type='s' name='Version' access='read'>"
	"      <annotation name='org.gtk.GDBus.Annotation' value='OnProperty'>"
	"        <annotation name='org.gtk.GDBus.Annotation' value='OnAnnotation_InterfaceVersion'/>"
	"      </annotation>"
	"    </property>"
	// message queue path
	"    <property type='s' name='QueuePath' access='read'>"
	"      <annotation name='org.gtk.GDBus.Annotation' value='OnProperty'>"
	"        <annotation name='org.gtk.GDBus.Annotation' value='OnAnnotation_InterfaceVersion'/>"
	"      </annotation>"
	"    </property>"
	"  </interface>"
	"</node>";

GDBusNodeInfo* introspection_data = NULL;

//---------------------------------------------------------------------------

static void
server_message_handler (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data);

static GVariant*
server_message_get (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data);
static gboolean
server_message_set (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data);

const GDBusInterfaceVTable server_vtable = {
	.method_call = server_message_handler,
	.get_property = server_message_get,
	.set_property = server_message_set
};

//---------------------------------------------------------------------------

pthread_t worker_id;
static void* dbus_worker (void*);
GMainLoop *worker_loop = NULL;

//===========================================================================

int bifrost_dbus_start_server ()
{
	if (worker_loop)	// already started
		return 0;

	// create worker thread
	if (pthread_create(&worker_id, NULL, dbus_worker, NULL))
	{
		syslog(LOG_CRIT, "failed to run d-bus thread!");
		return -1;
	}

	syslog (LOG_DEBUG, "dbus thread started");
	return 0;
}

void bifrost_dbus_stop_server ()
{
	g_main_loop_quit(worker_loop);
	pthread_join (worker_id, NULL);
	g_main_loop_unref(worker_loop);
	syslog (LOG_DEBUG, "dbus thread stopped");
}

//--------------------------------------------------------------------------
// D-Bus connection Callbacks -- called in worker thread
// these functions are called in orders, specific for each cases
// see g_bus_own_name documentation for details

GDBusConnection* dbus_connection = NULL;

// 'Shutdown' signal emitter
/*
gboolean emit_signal_shutdown (void* arg)
{
	if (dbus_connection)
	{
		
	}
}
*/

static void on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	// all object exports goes here
	guint registration_id;
	GError* error = NULL;

	registration_id = g_dbus_connection_register_object (connection,
                                                       DBUS_OBJECT,
                                                       introspection_data->interfaces[0],
                                                       &server_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       &error); /* GError** */
	if (error)
		syslog (LOG_ERR, "failed to register object!");
	else 
	{
		syslog (LOG_DEBUG, "object registered!");
	}
}

static void on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	syslog (LOG_DEBUG, "acquired name %s", name);
	dbus_connection = connection;
}

static void on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
	syslog (LOG_DEBUG, "lost name %s", name);
	dbus_connection = NULL;
  	exit (1);
}

//---------------------------------------------------------------------
// worker thread function

static void* dbus_worker (void* arg)
{
	guint owner_id = 0;
	GError* error = NULL;
	GMainContext *ctx = NULL;

	// we must create a separate context and main loop for our worker to process events
	ctx = g_main_context_new();

	if (!ctx)
	{
		syslog(LOG_CRIT, "failed to create main context!");
		pthread_exit(NULL);
	}

	// set new context for this thread
	g_main_context_push_thread_default(ctx);

	// create worker main loop
	worker_loop = g_main_loop_new(ctx, FALSE);
	if (!worker_loop)
	{
		syslog(LOG_CRIT, "failed to create main loop!");
		g_main_context_unref(ctx);
		pthread_exit(NULL);
	}

	// generate introspection data from xml
	introspection_data = g_dbus_node_info_new_for_xml (server_introspection_xml, &error);
	if (error || introspection_data == NULL)
	{
		syslog (LOG_ERR, "failed to create introspection data for D-Bus server!");
		g_main_context_unref(ctx);
		pthread_exit(NULL);
	}

	// connecting to D-Bus
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             DBUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_REPLACE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);
	
	if (owner_id > 0)
	{
		syslog (LOG_DEBUG, "preparing to own name");

		// connection successful => processing all events from D-Bus
		g_main_loop_run (worker_loop);

		syslog (LOG_DEBUG, "worker-thread: loop end");

		// emit shutdown signal
		g_dbus_connection_emit_signal (dbus_connection,
						NULL,	// broadcast signal
						DBUS_OBJECT,
						DBUS_INTERFACE,
						"Shutdown",
						NULL,
						&error);
		if (error)
			syslog (LOG_ERR, "failed to transmit signal: %s", error->message);

		// free D-Bus resource
		g_bus_unown_name (owner_id);
	} else 
		syslog (LOG_ERR, "failed to own name "DBUS_NAME);

	g_dbus_node_info_unref (introspection_data);
	if (error)
		g_error_free (error);

	pthread_exit(NULL);
}

//------------------------------------------------------------------------------------
// server callbacks - sent from D-Bus, processed in worker thread

static void server_message_handler (GDBusConnection       *connection,
	            const gchar           *sender,
	            const gchar           *object_path,
	            const gchar           *interface_name,
	            const gchar           *method_name,
	            GVariant              *parameters,
	            GDBusMethodInvocation *invocation,
	            gpointer               user_data)
{
	if (g_strcmp0 (method_name, "RegisterUnit") == 0)
	{
		char* id = NULL;
		unsigned int requested_packet_size = 0;
		int result = 0;
		channel_info_t info = { 0, NULL };

		// get arguments
		syslog (LOG_DEBUG, "processing %s call", method_name);

		g_variant_get (parameters, "(&su)", &id, &requested_packet_size);
		result = register_unit (id, requested_packet_size, &info);

		if (result != 0)
		{
			syslog (LOG_ERR, "Failed to register unit id %s, requested %d bytes",
					sender, requested_packet_size);
			// return error
			if (result == -2)
				g_dbus_method_invocation_return_error (invocation,
							      G_DBUS_ERROR,
	                                                      G_DBUS_ERROR_NO_MEMORY,
	                                                      "Failed to allocate requested resources!");
			else if (result == -1)
				g_dbus_method_invocation_return_error (invocation,
							      G_DBUS_ERROR,
	                                                      G_DBUS_ERROR_INVALID_ARGS,
	                                                      "Invalid arguments");

			g_dbus_method_invocation_return_dbus_error (invocation,
								"org.malckhazar.UnknownError",
								"Unknown error occured on unit registration!");
			return;
		}
		syslog (LOG_DEBUG, "register_unit returned: '%d', '%s', '%s'", info.queue_id, info.shm_name, info.sem_name);

		// response
		g_dbus_method_invocation_return_value (invocation,
							g_variant_new ("(i&s&s)",
									info.queue_id,
									info.shm_name ? info.shm_name : "",
									info.sem_name ? info.sem_name : ""));

		return;
	} else if (g_strcmp0 (method_name, "UnregisterUnit") == 0)
	{
		char* id = NULL;
		syslog (LOG_DEBUG, "processing %s call", method_name);
		g_variant_get (parameters, "(&s)", &id);

		unregister_unit (id);
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
	syslog (LOG_WARNING, "Unhandled method call: %s", method_name);
}

static GVariant*
server_message_get (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	GVariant *ret;

	ret = NULL;

	if (g_strcmp0 (property_name, "Version") == 0)
	{
		ret = g_variant_new_string (version);
	} else if (g_strcmp0 (property_name, "QueuePath") == 0)
	{
		ret = g_variant_new_string (settings.queue_path);
	}

	return ret;
}

static gboolean
server_message_set (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	return FALSE;
}

