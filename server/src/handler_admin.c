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
#include <bidib/bidib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "server.h"
#include "param_verification.h"
#include "handler_upload.h"
#include "handler_driver.h"
#include "handler_controller.h"
#include "interlocking.h"
#include "dyn_containers_interface.h"
#include "bahn_data_util.h"

static pthread_mutex_t start_stop_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t poll_bidib_messages_thread;


void build_message_hex_string(unsigned char *message, char *dest) {
	for (size_t i = 0; i <= message[0]; i++) {
		if (i != 0) {
			dest += sprintf(dest, " ");
		}
		dest += sprintf(dest, "0x%02x", message[i]);
	}
}

static void *poll_bidib_messages(void *_) {
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

// Must be called with start_stop_mutex already acquired
static bool start_bidib(void) {
	const int err_serial = bidib_start_serial(serial_device, config_directory, 0);
	if (err_serial) {
		syslog_server(LOG_ERR, "Request: Start - Could not start BiDiB serial connection");
		return false;
	}
	
	const int succ_clear_dir = clear_engine_dir() + clear_interlocker_dir();
	if (!succ_clear_dir) {
		syslog_server(LOG_ERR, "Request: Start - Could not clear the engine and interlocker directories");
		return false;
	}

	const int succ_config = bahn_data_util_initialise_config(config_directory);
	if (!succ_config) {
		syslog_server(LOG_ERR, "Request: Start - Could not initialise interlocking tables");
		return false;
    }

	const int err_dyn_containers = dyn_containers_start();
	if (err_dyn_containers) {
		syslog_server(LOG_ERR, "Request: Start - Could not start shared library containers");
		return false;
	}
	
	const int err_interlocker = load_default_interlocker_instance();
	if (err_interlocker) {
		syslog_server(LOG_ERR, "Request: Start - Could not load default interlocker instance");
		return false;
	}
	
	running = true;
	pthread_create(&poll_bidib_messages_thread, NULL, poll_bidib_messages, NULL);	
	return true;
}

void stop_bidib(void) {
	session_id = 0;
	syslog_server(LOG_NOTICE, "Request: Stop");
	release_all_grabbed_trains();
	release_all_interlockers();
	running = false;
	dyn_containers_stop();
	bahn_data_util_free_config();
	pthread_join(poll_bidib_messages_thread, NULL);
	bidib_stop();
}

onion_connection_status handler_startup(void *_, onion_request *req,
                                        onion_response *res) {
	build_response_header(res);
	int retval = OCS_NOT_IMPLEMENTED;
	
	pthread_mutex_lock(&start_stop_mutex);
	if (!running && ((onion_request_get_flags(req) &
	                                           OR_METHODS) == OR_POST)) {
		// Necessary when restarting the server because libbidib closes syslog on exit
		openlog("swtbahn", 0, LOG_LOCAL0);	

		session_id = time(NULL);
		syslog_server(LOG_NOTICE, "Request: Start, session id: %ld", session_id);

		if (start_bidib()) { 
			retval = OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Start - BiDiB system is already running");
	}
	pthread_mutex_unlock(&start_stop_mutex);

	return retval;
}

onion_connection_status handler_shutdown(void *_, onion_request *req,
                                         onion_response *res) {
	build_response_header(res);
	int retval = OCS_NOT_IMPLEMENTED;
	
	pthread_mutex_lock(&start_stop_mutex);
	if (running && ((onion_request_get_flags(req) &
	                                          OR_METHODS) == OR_POST)) {
		stop_bidib();
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
		if (data_state == NULL || (state == LONG_MAX || state == LONG_MIN) ||
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

onion_connection_status handler_admin_release_train(void *_, onion_request *req,
                                                    onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {		
		const char *data_train = onion_request_get_post(req, "train");
		const int grab_id = train_get_grab_id(data_train);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Admin release train - invalid train id " 
			              "or train not grabbed (%s)",
			              data_train);
			return OCS_NOT_IMPLEMENTED;
		}
		
		// Ensure that the train has stopped moving
		pthread_mutex_lock(&grabbed_trains_mutex);	
		const int engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
		dyn_containers_set_train_engine_instance_inputs(engine_instance, 0, true);
		pthread_mutex_unlock(&grabbed_trains_mutex);
		
		t_bidib_train_state_query train_state_query = bidib_get_train_state(grabbed_trains[grab_id].name->str);
		while (train_state_query.data.set_speed_step != 0) {
			bidib_free_train_state_query(train_state_query);
			train_state_query = bidib_get_train_state(grabbed_trains[grab_id].name->str);
		}
		bidib_free_train_state_query(train_state_query);
		
		if (!release_train(grab_id)) {
			syslog_server(LOG_ERR, "Request: Admin release train - invalid grab id");
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, "Request: Admin release train");
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Admin release train - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_admin_set_dcc_train_speed(void *_, onion_request *req,
                                                          onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const char *data_speed = onion_request_get_post(req, "speed");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		const int grab_id = train_get_grab_id(data_train);
		int speed = params_check_speed(data_speed);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Admin set train speed - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (speed == 999) {
			syslog_server(LOG_ERR, "Request: Admin set train speed - bad speed");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, "Request: Admin set train speed - bad track output");
			return OCS_NOT_IMPLEMENTED;
		} else {
			pthread_mutex_lock(&grabbed_trains_mutex);
			strcpy(grabbed_trains[grab_id].track_output, data_track_output);
			int dyn_containers_engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
			if (speed < 0) {
				dyn_containers_set_train_engine_instance_inputs(dyn_containers_engine_instance,
				                                                -speed, false);
			} else {
				dyn_containers_set_train_engine_instance_inputs(dyn_containers_engine_instance,
				                                                speed, true);
			}
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Admin set train speed - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
