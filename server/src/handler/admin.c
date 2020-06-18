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
#include <stdio.h>

#include "../server.h"
#include "driver.h"
#include "../interlocking/interlocking.h"
#include "../dyn_containers_interface.h"
#include "../util_handler.h"
#include "admin.h"


static pthread_mutex_t start_stop_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t start_stop_thread;


void build_message_hex_string(unsigned char *message, char *dest) {
	for (size_t i = 0; i <= message[0]; i++) {
		if (i != 0) {
			dest += sprintf(dest, " ");
		}
		dest += sprintf(dest, "0x%02x", message[i]);
	}
}

static void *start_bidib(void *_) {
    char* directory = malloc(sizeof(char) * (strlen(config_directory) + strlen(platform_loaded) + 1));
    strcpy(directory, config_directory);
    directory = strcat(directory, "/");
    directory = strcat(directory, platform_loaded);

	int err_serial = bidib_start_serial(serial_device, directory, 0);
	if (err_serial) {
		pthread_mutex_lock(&start_stop_mutex);
		starting = false;
		pthread_mutex_unlock(&start_stop_mutex);
		pthread_exit(NULL);
	}
	
	int err_interlocking = interlocking_table_initialise(directory);
	if (err_interlocking) {
		pthread_mutex_lock(&start_stop_mutex);
		starting = false;
		pthread_mutex_unlock(&start_stop_mutex);
		pthread_exit(NULL);
	}

	free(directory);
	
	int err_dyn_containers = dyn_containers_start();
	if (err_dyn_containers) {
		pthread_mutex_lock(&start_stop_mutex);
		starting = false;
		pthread_mutex_unlock(&start_stop_mutex);
		pthread_exit(NULL);
	}
	
	pthread_mutex_lock(&start_stop_mutex);
	running = true;
	starting = false;
	pthread_mutex_unlock(&start_stop_mutex);
	while (running) {
		unsigned char *message;
		while ((message = bidib_read_message()) != NULL) {
			char hex_string[(message[0] + 1) * 5];
			build_message_hex_string(message, hex_string);
			syslog_server(LOG_NOTICE, "SWTbahn message queue: %s", hex_string);
			free(message);
		}
		while ((message = bidib_read_error_message()) != NULL) {
			char hex_string[(message[0] + 1) * 5];
			build_message_hex_string(message, hex_string);
			syslog_server(LOG_ERR, "SWTbahn error message queue: %s", hex_string);
			free(message);
		}
		usleep(500000);	// 0.5 seconds
	}
	pthread_exit(NULL);
}

onion_connection_status handler_startup(void *_, onion_request *req,
                                        onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    const char* platform = onion_request_get_post(req, "platform");
    if (platform_loaded != NULL) {
        free(platform_loaded);
    }
    platform_loaded = malloc(sizeof(char) * strlen(platform));
    strcpy(platform_loaded, platform);


	build_response_header(res);
	int retval;
	pthread_mutex_lock(&start_stop_mutex);
	if (!running && !starting && !stopping) {
		session_id = time(NULL);
		syslog_server(LOG_NOTICE, "Request: Start, session id: %ld", session_id);
		starting = true;
		pthread_create(&start_stop_thread, NULL, start_bidib, NULL);
		retval = OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Start - BiDiB system is already running");
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
	free_interlocking_table();
	pthread_mutex_lock(&start_stop_mutex);
	stopping = false;
	pthread_mutex_unlock(&start_stop_mutex);
	return NULL;
}

onion_connection_status handler_shutdown(void *_, onion_request *req,
                                         onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    if (platform_loaded != NULL) {
        free(platform_loaded);
        platform_loaded = NULL;
    }

	build_response_header(res);
	int retval;
	pthread_mutex_lock(&start_stop_mutex);
	if (running && !starting && !stopping) {
		syslog_server(LOG_NOTICE, "Request: Stop");
		stopping = true;
		running = false;
		dyn_containers_stop();
		pthread_join(start_stop_thread, NULL);
		pthread_create(&start_stop_thread, NULL, stop_bidib, NULL);
		retval = OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Stop - BiDiB system is not running");
		retval = OCS_NOT_IMPLEMENTED;
	}
	pthread_mutex_unlock(&start_stop_mutex);
	return retval;
}

onion_connection_status handler_set_track_output(void *_, onion_request *req,
                                                 onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		char *end;
		const char *data_state = onion_request_get_post(req, "state");
		long int state = strtol(data_state, &end, 10);
		if (data_state == NULL || (state == 0L || state == LONG_MAX || state == LONG_MIN) ||
		    *end != '\0') {
			syslog_server(LOG_ERR, "Request: Set track output - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, "Request: Set track output - state: 0x%02x", state);
			bidib_set_track_output_state_all(state);
			bidib_flush();
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set track output - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

