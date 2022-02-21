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
 * - Tri Nguyen <https://github.com/trinnguyen>
 *
 */

#include <onion/onion.h>
#include <bidib.h>
#include <pthread.h>
#include <string.h>

#include "server.h"
#include "dyn_containers_interface.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

static t_interlocker_data interlocker_instances[INTERLOCKER_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_interlocker_instance = -1 }
};

static GString *selected_interlocker_name;
static int selected_interlocker_instance = -1;


const int set_interlocker(const char *interlocker_name) {
	if (selected_interlocker_instance != -1) {
		return selected_interlocker_instance;
	}
	
	pthread_mutex_lock(&interlocker_mutex);
	for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		if (!interlocker_instances[i].is_valid) {
			if (dyn_containers_set_interlocker_instance(
					&interlocker_instances[i], interlocker_name)
			) {
				syslog_server(LOG_ERR, "Interlocker %s could not be used in instance %d", 
							  interlocker_name, i);
			} else {
				selected_interlocker_name = g_string_new(interlocker_name);
				selected_interlocker_instance = i;
				interlocker_instances[selected_interlocker_instance].is_valid = true;
			}
			break;
		}
	}
	pthread_mutex_unlock(&interlocker_mutex);
	return selected_interlocker_instance;
}

const int unset_interlocker(const char *interlocker_name) {
	if (selected_interlocker_instance == -1) {
		return selected_interlocker_instance;
	}
	
	pthread_mutex_lock(&interlocker_mutex);
	if (strcmp(selected_interlocker_name->str, interlocker_name) == 0
			&& interlocker_instances[selected_interlocker_instance].is_valid) {
		dyn_containers_free_interlocker_instance(&interlocker_instances[selected_interlocker_instance]);
		interlocker_instances[selected_interlocker_instance].is_valid = false;
		g_string_free(selected_interlocker_name, true);
		selected_interlocker_instance = -1;
	}
	pthread_mutex_unlock(&interlocker_mutex);
	return selected_interlocker_instance;
}

const int load_default_interlocker_instance() {
	while (!dyn_containers_is_running()) {
		// Empty
	}

	selected_interlocker_name = g_string_new("libinterlocker_default (unremovable)");	
	const int result = set_interlocker(selected_interlocker_name->str);
	return (result == -1);
}

void free_all_interlockers(void) {
	if (selected_interlocker_name != NULL) {
		g_string_free(selected_interlocker_name, true);
	}

	for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		pthread_mutex_lock(&interlocker_mutex);
		if (interlocker_instances[i].is_valid) {
			dyn_containers_free_interlocker_instance(&interlocker_instances[i]);
			interlocker_instances[i].is_valid = false;
		}
		pthread_mutex_unlock(&interlocker_mutex);
	}
	
	selected_interlocker_instance = -1;
}

const char *grant_route(const char *train_id, const char *source_id, const char *destination_id) {
	if (selected_interlocker_instance == -1) {
		syslog_server(LOG_ERR, "Grant route: No interlocker has been set");
		return "no_interlocker";
	}
	
	pthread_mutex_lock(&interlocker_mutex);

	bahn_data_util_init_cached_track_state();
	
	// Ask the interlocker to grant requested route.
	// May take multiple ticks to process the request.
	dyn_containers_set_interlocker_instance_inputs(&interlocker_instances[selected_interlocker_instance], 
                                                   source_id, destination_id, 
                                                   train_id);

	struct t_interlocker_instance_io interlocker_instance_io;	
	do {
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[selected_interlocker_instance],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_has_reset);

	dyn_containers_set_interlocker_instance_reset(&interlocker_instances[selected_interlocker_instance], 
	                                              false);
	
	do {
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[selected_interlocker_instance],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_terminated);
	
	bahn_data_util_free_cached_track_state();

	// Return the result
	const char *route_id = interlocker_instance_io.output_route_id;
	if (route_id != NULL && params_check_is_number(route_id)) {
		syslog_server(LOG_NOTICE, "Grant route: Route %s has been granted", route_id);
		
		syslog_server(LOG_NOTICE, "Request: Set points and signals for route id \"%s\" - interlocker type %d",
		              interlocker_instance_io.output_route_id,
		              interlocker_instance_io.output_interlocker_type);
	} else {
		if (strcmp(route_id, "no_routes") == 0) {
			syslog_server(LOG_ERR, "Grant route: No routes possible from %s to %s", source_id, destination_id);
		} else if (strcmp(route_id, "not_grantable") == 0) {
			syslog_server(LOG_ERR, "Grant route: Conflicting routes are in use");
		} else if (strcmp(route_id, "not_clear") == 0) {
			syslog_server(LOG_ERR, "Grant route: Route found has occupied blocks or source signal is not stop");
		} else {
			syslog_server(LOG_ERR, "Grant route: Route could not be granted (%s)", route_id);
		}
	}

	pthread_mutex_unlock(&interlocker_mutex);
	return route_id;
}

void release_route(const int route_id) {
	pthread_mutex_lock(&interlocker_mutex);
	t_interlocking_route *route = get_route(route_id);
	if (route->train != NULL) {
		const char *signal_id = route->source;
		const char *signal_aspect = "red";
		if (bidib_set_signal(signal_id, signal_aspect)) {
			syslog_server(LOG_ERR, "Release route: Unable to set entry signal to aspect %s", signal_aspect);
		}
		bidib_flush();
		
		free(route->train);
		route->train = NULL;
		syslog_server(LOG_NOTICE, "Release route: route %d released", route_id);
    }

	pthread_mutex_unlock(&interlocker_mutex);
}

onion_connection_status handler_release_route(void *_, onion_request *req,
                                          onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const int route_id = params_check_route_id(data_route_id);
		if (route_id == -1) {
			syslog_server(LOG_ERR, "Request: Release route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			release_route(route_id);
			syslog_server(LOG_NOTICE, "Request: Release route - route: %d", route_id);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Release route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_point == NULL || data_state == NULL) {
			syslog_server(LOG_ERR, "Request: Set point - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_switch_point(data_point, data_state)) {
				syslog_server(LOG_ERR, "Request: Set point - invalid parameters");
				bidib_flush();
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog_server(LOG_NOTICE, "Request: Set point - point: %s state: %s",
				       data_point, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set point - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_signal = onion_request_get_post(req, "signal");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_signal == NULL || data_state == NULL) {
			syslog_server(LOG_ERR, "Request: Set signal - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_set_signal(data_signal, data_state)) {
				syslog_server(LOG_ERR, "Request: Set signal - invalid parameters");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog_server(LOG_NOTICE, "Request: Set signal - signal: %s state: %s",
				              data_signal, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set signal - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_peripheral(void *_, onion_request *req,
                                               onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_peripheral = onion_request_get_post(req, "peripheral");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_peripheral == NULL || data_state == NULL) {
			syslog_server(LOG_ERR, "Request: Set peripheral - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_set_peripheral(data_peripheral, data_state)) {
				syslog_server(LOG_ERR, "Request: Set peripheral - invalid parameters");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog_server(LOG_NOTICE, "Request: Set peripheral - peripheral: %s state: %s",
				              data_peripheral, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set peripheral - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_interlocker(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		if (selected_interlocker_instance != -1) {
			onion_response_printf(res, "%s", selected_interlocker_name->str);
			return OCS_PROCESSED;
		} else {
			syslog_server(LOG_NOTICE, "Request: Get interlocker - none selected");
			return OCS_NOT_IMPLEMENTED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get interlocker - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_interlocker(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_interlocker = onion_request_get_post(req, "interlocker");
		if (data_interlocker == NULL) {
			syslog_server(LOG_ERR, "Request: Set interlocker - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (selected_interlocker_instance != -1) {
				syslog_server(LOG_ERR, "Request: Set interlocker - another interlocker instance already set");
				return OCS_NOT_IMPLEMENTED;
			}

			set_interlocker(data_interlocker);
			if (selected_interlocker_instance == -1) {
				syslog_server(LOG_ERR, "Request: Set interlocker - invalid parameters or "
				                       "no more interlocker instances can be loaded");
				return OCS_NOT_IMPLEMENTED;
			} else {
				onion_response_printf(res, "%s", selected_interlocker_name->str);
				syslog_server(LOG_NOTICE, "Request: Set interlocker - %s",
				              selected_interlocker_name->str);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set interlocker - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_unset_interlocker(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_interlocker = onion_request_get_post(req, "interlocker");
		if (data_interlocker == NULL) {
			syslog_server(LOG_ERR, "Request: Unset interlocker - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (selected_interlocker_instance == -1) {
				syslog_server(LOG_ERR, "Request: Unset interlocker - no interlocker instance to unset");
				return OCS_NOT_IMPLEMENTED;
			}
			
			unset_interlocker(data_interlocker);
			if (selected_interlocker_instance != -1) {
				syslog_server(LOG_ERR, "Request: Unset interlocker - invalid parameters");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog_server(LOG_NOTICE, "Request: Unset interlocker - %s",
				              data_interlocker);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Unset interlocker - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
