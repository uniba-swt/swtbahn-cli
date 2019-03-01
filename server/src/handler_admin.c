/*
 *
 * Copyright (C) 2017 University of Bamberg, Software Technologies Research Group
 * <https://www.uni-bamberg.de/>, <http://www.swt-bamberg.de/>
 * 
 * This file is part of the SWTbahn command line interface (swtbahn-cli), which is
 * a client-server application to interactively control a BiDiB model railway.
 *
 * swtbahn-cli is licensed under the GNU GENERAL PUBLIC LICENSE (Version 3), see
 * the LICENSE file at the project's top-level directory for details or consult
 * <http://www.gnu.org/licenses/>.
 *
 * swtbahn-cli is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * swtbahn-cli is a RESEARCH PROTOTYPE and distributed WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * The following people contributed to the conception and realization of the
 * present swtbahn-cli (in alphabetic order by surname):
 *
 * - Nicolas Gross <https://github.com/nicolasgross>
 *
 */

#include <onion/onion.h>
#include <bidib.h>
#include <pthread.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>

#include "server.h"
#include "handler_driver.h"
#include "interlocking.h"


pthread_mutex_t start_stop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t start_stop_thread;


void build_message_hex_string(unsigned char *message, char *dest) {
	for (size_t i = 0; i <= message[0]; i++) {
		if (i != 0) {
			dest += sprintf(dest, " ");
		}
		dest += sprintf(dest, "0x%02x", message[i]);
	}
}

static void *start_bidib(void *_) {
	int err_serial = bidib_start_serial(serial_device, config_directory, 0);
	int err_interlocking = interlocking_table_initialise();
	pthread_mutex_lock(&start_stop_mutex);
	if (err_serial || err_interlocking) {
		starting = false;
		pthread_mutex_unlock(&start_stop_mutex);
	} else {
		running = true;
		starting = false;
		pthread_mutex_unlock(&start_stop_mutex);
		while (running) {
			unsigned char *message;
			while ((message = bidib_read_message()) != NULL) {
				char hex_string[message[0] * 5];
				build_message_hex_string(message, hex_string);
				syslog(LOG_NOTICE, "SWTbahn message queue: %s", hex_string);
				free(message);
			}
			while ((message = bidib_read_error_message()) != NULL) {
				char hex_string[message[0] * 5];
				build_message_hex_string(message, hex_string);
				syslog(LOG_ERR, "SWTbahn error message queue: %s", hex_string);
				free(message);
			}
			usleep(500000);
		}
	}
	return NULL;
}

onion_connection_status handler_startup(void *_, onion_request *req,
                                        onion_response *res) {
	int retval;
	pthread_mutex_lock(&start_stop_mutex);
	if (!running && !starting && !stopping && ((onion_request_get_flags(req) &
	                                           OR_METHODS) == OR_POST)) {
		session_id = time(NULL);
		syslog(LOG_NOTICE, "Request: Start, session id: %ld", session_id);
		starting = true;
		pthread_create(&start_stop_thread, NULL, start_bidib, NULL);
		retval = OCS_PROCESSED;
	} else {
		syslog(LOG_ERR, "Request: Start - BiDiB system is already running");
		retval = OCS_NOT_IMPLEMENTED;
	}
	pthread_mutex_unlock(&start_stop_mutex);
	return retval;
}

static void *stop_bidib(void *_) {
	usleep (1000000); // wait for running functions
	bidib_stop();
	free_all_grabbed_trains();
	free_interlocking_hashtable();
	pthread_mutex_lock(&start_stop_mutex);
	stopping = false;
	pthread_mutex_unlock(&start_stop_mutex);
	return NULL;
}

onion_connection_status handler_shutdown(void *_, onion_request *req,
                                         onion_response *res) {
	int retval;
	pthread_mutex_lock(&start_stop_mutex);
	if (running && !starting && !stopping && ((onion_request_get_flags(req) &
	                                          OR_METHODS) == OR_POST)) {
		syslog(LOG_NOTICE, "Request: Stop");
		stopping = true;
		running = false;
		pthread_create(&start_stop_thread, NULL, stop_bidib, NULL);
		retval = OCS_PROCESSED;
	} else {
		syslog(LOG_ERR, "Request: Stop - BiDiB system is not running");
		retval = OCS_NOT_IMPLEMENTED;
	}
	pthread_mutex_unlock(&start_stop_mutex);
	return retval;
}

onion_connection_status handler_set_track_output(void *_, onion_request *req,
                                                 onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		unsigned int state;
		char *end;
		const char *data_state = onion_request_get_post(req, "state");
		if (data_state == NULL || (state = strtol(data_state, &end, 10)) < 0 ||
		    *end != '\0') {
			syslog(LOG_ERR, "Request: Set track output - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog(LOG_NOTICE, "Request: Set track output - state: 0x%02x", state);
			bidib_set_track_output_state_all(state);
			bidib_flush();
			return OCS_PROCESSED;
		}
	} else {
		syslog(LOG_ERR, "Request: Set track output - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

