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

#include <bidib/bidib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "handler_admin.h"
#include "server.h"
#include "handler_upload.h"
#include "handler_driver.h"
#include "handler_controller.h"
#include "dyn_containers_interface.h"
#include "param_verification.h"
#include "bahn_data_util.h"
#include "websocket_uploader/engine_uploader.h"

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
static bool startup_server(void) {
	const int err_serial = bidib_start_serial(serial_device, config_directory, 0);
	if (err_serial) {
		syslog_server(LOG_ERR, "Startup server - Could not start BiDiB serial connection");
		return false;
	}
	
	const int succ_clear_dir = clear_engine_dir() + clear_interlocker_dir();
	if (!succ_clear_dir) {
		syslog_server(LOG_ERR, 
		              "Startup server - Could not clear the engine and interlocker directories");
		return false;
	}

	const int succ_config = bahn_data_util_initialise_config(config_directory);
	if (!succ_config) {
		syslog_server(LOG_ERR, "Startup server - Could not initialise interlocking tables");
		return false;
	}

	const int err_dyn_containers = dyn_containers_start();
	if (err_dyn_containers) {
		syslog_server(LOG_ERR, "Startup server - Could not start shared library containers");
		return false;
	}
	
	const int err_interlocker = load_default_interlocker_instance();
	if (err_interlocker) {
		syslog_server(LOG_ERR, "Startup server - Could not load default interlocker instance");
		return false;
	}
	
	running = true;
	pthread_create(&poll_bidib_messages_thread, NULL, poll_bidib_messages, NULL);	
	return true;
}

void shutdown_server(void) {
	session_id = 0;
	syslog_server(LOG_NOTICE, "Shutdown server");
	release_all_grabbed_trains();
	syslog_server(LOG_INFO, "Shutdown server - Released all grabbed trains");
	release_all_interlockers();
	syslog_server(LOG_INFO, "Shutdown server - Released all interlockers");
	running = false;
	dyn_containers_stop();
	syslog_server(LOG_INFO, "Shutdown server - Stopped dyn containers");
	bahn_data_util_free_config();
	syslog_server(LOG_INFO, "Shutdown server - Released interlocking config and table data");
	pthread_join(poll_bidib_messages_thread, NULL);
	syslog_server(LOG_NOTICE, 
	              "Shutdown server - BiDiB message poll thread joined, now stopping BiDiB and closing log");
	bidib_stop();
}

onion_connection_status handler_startup(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	int retval = OCS_NOT_IMPLEMENTED;
	
	pthread_mutex_lock(&start_stop_mutex);
	if (!running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		// Necessary when restarting the server because libbidib closes syslog on exit
		openlog("swtbahn", 0, LOG_LOCAL0);

		session_id = time(NULL);
		syslog_server(LOG_NOTICE, "Request: Startup server - session id: %ld - start", session_id);

		if (startup_server()) { 
			retval = OCS_PROCESSED;
			syslog_server(LOG_NOTICE, 
			              "Request: Startup server - session id: %ld - finish", 
			              session_id);
		}  else {
			syslog_server(LOG_ERR, 
			              "Request: Startup server - session id: %ld - unable to start BiDiB - abort", 
			              session_id);
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Startup server - BiDiB system is already running or wrong request type");
	}
	pthread_mutex_unlock(&start_stop_mutex);

	return retval;
}

onion_connection_status handler_shutdown(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	int retval = OCS_NOT_IMPLEMENTED;
	
	pthread_mutex_lock(&start_stop_mutex);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_NOTICE, "Request: Shutdown server - start");
		shutdown_server();
		// Can't log "finished" here since bidib closes the syslog when stopping
		retval = OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Shutdown server - BiDiB system is not running or wrong request type");
		retval = OCS_NOT_IMPLEMENTED;
	}
	pthread_mutex_unlock(&start_stop_mutex);
	
	return retval;
}

onion_connection_status handler_set_track_output(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		char *end;
		const char *data_state = onion_request_get_post(req, "state");
		long int state = strtol(data_state, &end, 10);
		if (data_state == NULL || (state == LONG_MAX || state == LONG_MIN) || *end != '\0') {
			syslog_server(LOG_ERR, "Request: Set track output - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, "Request: Set track output - state: 0x%02x - start", state);
			bidib_set_track_output_state_all(state);
			bidib_flush();
			syslog_server(LOG_NOTICE, "Request: Set track output - state: 0x%02x - finish", state);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Set track output - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_verification_option(void *_, onion_request *req,
                                                        onion_response *res) {
	build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_POST) {
		const char *data_verification_option = onion_request_get_post(req, "verification-option");
		if (!params_check_is_bool_string(data_verification_option)) {
			syslog_server(LOG_ERR, "Request: Set verification option - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		if (strcmp("true", data_verification_option) == 0 
			|| strcmp("True", data_verification_option) == 0 
			|| strcmp("TRUE", data_verification_option) == 0) {
			verification_enabled = true;
		} else {
			verification_enabled = false;
		}
		syslog_server(LOG_NOTICE, 
		              "Request: Set verification option - new state: %s - done", 
		              verification_enabled ? "enabled" : "disabled");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Set verification option - wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_verification_url(void *_, onion_request *req,
                                                     onion_response *res) {
    build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_POST) {
		const char *data_verification_url = onion_request_get_post(req, "verification-url");
		if (data_verification_url == NULL) {
			syslog_server(LOG_ERR, "Request: Set verification URL - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		set_verifier_url(data_verification_url);
		syslog_server(LOG_NOTICE, 
		              "Request: Set verification URL - new URL: %s - done", 
		              data_verification_url);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Set verification URL - wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_admin_release_train(void *_, onion_request *req,
                                                    onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const int grab_id = train_get_grab_id(data_train);
		
		pthread_mutex_lock(&grabbed_trains_mutex);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			syslog_server(LOG_ERR, 
			              "Request: Admin release train - invalid train id or train %s not grabbed",
			              data_train);
			return OCS_NOT_IMPLEMENTED;
		}
		syslog_server(LOG_NOTICE, "Request: Admin release train - train: %s - start", data_train);
		
		// Ensure that the train has stopped moving
		const int engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
		dyn_containers_set_train_engine_instance_inputs(engine_instance, 0, true);
		pthread_mutex_unlock(&grabbed_trains_mutex);
		
		
		t_bidib_train_state_query train_state_query = bidib_get_train_state(data_train);
		while (train_state_query.data.set_speed_step != 0) {
			bidib_free_train_state_query(train_state_query);
			usleep(TRAIN_DRIVE_TIME_STEP); // 0.01 s
			train_state_query = bidib_get_train_state(data_train);
		}
		bidib_free_train_state_query(train_state_query);
		
		
		if (!release_train(grab_id)) {
			syslog_server(LOG_ERR, 
			              "Request: Admin release train - train: %s - invalid grab id - abort", 
			              data_train);
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, 
			              "Request: Admin release train - train: %s - finish",
			              data_train);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Admin release train - system not running or wrong request type");
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
		int speed = params_check_speed(data_speed);
		
		if (speed == 999) {
			syslog_server(LOG_ERR, 
			              "Request: Admin set dcc train speed - train: %s speed: %d - invalid speed",
			              data_train, speed);
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, 
			              "Request: Admin set dcc train speed - train: %s speed: %d - invalid track output", 
			              data_train, speed);
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, 
			              "Request: Admin set dcc train speed - train: %s speed: %d - start", 
			              data_train, speed);
			pthread_mutex_lock(&grabbed_trains_mutex);
			if (bidib_set_train_speed(data_train, speed, data_track_output)) {
				syslog_server(LOG_ERR, 
				              "Request: Admin set dcc train speed - train: %s speed: %d - "
				              "invalid parameters - abort", 
				              data_train, speed);
			} else {
				bidib_flush();
				syslog_server(LOG_NOTICE, 
				              "Request: Admin set dcc train speed - train: %s speed: %d - finish", 
				              data_train, speed);
			}
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Admin set dcc train speed - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
