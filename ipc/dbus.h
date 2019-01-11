/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * lib.h
 * Copyright (C) 2019 Anton Sirazetdinov <malckhazar@gmail.com>
 * 
 * libdn-ipc is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libdn-ipc is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.";
 */

#include <sys/types.h>

#define DBUS_NAME 		"org.malckhazar.bifrost"
#define DBUS_OBJECT 	"/org/malckhazar/bifrost/object"
#define DBUS_INTERFACE 	"org.malckhazar.Bifrost"

// from dbus_server.c
int bifrost_dbus_start_server ();
void bifrost_dbus_stop_server ();

typedef enum signal_type_t {
	BIFROST_SIGNAL_SHUTDOWN = 0,
	BIFROST_SIGNAL_CHANNEL_REGISTERED
} signal_type_t;

/* signal arguments:
		BIFROST_SIGNAL_SHUTDOWN: (none)
		BIFROST_SIGNAL_CHANNEL_REGISTERED: name, queue id, shm path, sem path
*/
void bifrost_dbus_emit_signal (signal_type_t signal_type, ...);

