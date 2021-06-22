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

// TODO: Turn into a proper structure for multiple interlocker instances
static t_interlocker_data interlocker_instances[INTERLOCKER_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_interlocker_instance = -1 }
};


void free_all_interlockers(void) {
	for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		if (interlocker_instances[i].is_valid) {
			dyn_containers_free_interlocker_instance(&interlocker_instances[i]);
			interlocker_instances[i].is_valid = false;
		}
	}
}

int load_default_interlocker_instance() {
	while (!dyn_containers_is_running()) {
		// Empty
	}
	
	const char interlocker_name[] = "libinterlocker_default (unremovable)";
	if (dyn_containers_set_interlocker_instance(&interlocker_instances[0], interlocker_name)) {
		syslog_server(LOG_ERR, "Interlocker %s could not be used", interlocker_name);
		return 0;
	}
	interlocker_instances[0].is_valid = true;
	
	return 1;
}

const char *grant_route(const char *train_id, const char *source_id, const char *destination_id) {
	pthread_mutex_lock(&interlocker_mutex);

	bahn_data_util_init_cached_track_state();
	
	// Ask the interlocker to grant requested route.
	// May take multiple ticks to process the request.
	dyn_containers_set_interlocker_instance_inputs(&interlocker_instances[0], 
                                                   source_id, destination_id, 
                                                   train_id);

	struct t_interlocker_instance_io interlocker_instance_io;	
	do {
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[0],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_has_reset);
	
	dyn_containers_set_interlocker_instance_reset(&interlocker_instances[0], 
	                                              false);
	
	do {
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[0],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_terminated);
	
	bahn_data_util_free_cached_track_state();

	// Return the result
	const char *route_id = interlocker_instance_io.output_route_id;
	if (route_id != NULL && params_check_is_number(route_id)) {
		syslog_server(LOG_NOTICE, "Grant route: Route %s has been granted", route_id);
		
		bidib_flush();
		syslog_server(LOG_NOTICE, "Request: Set points and signals for route id \"%s\" - interlocker type %d",
		              interlocker_instance_io.output_route_id,
		              interlocker_instance_io.output_interlocker_type);
	} else {
		if (strcmp(route_id, "no_routes") == 0) {
			syslog_server(LOG_ERR, "Grant route: No routes possible from %s to %s", source_id, destination_id);
		} else if (strcmp(route_id, "not_grantable") == 0) {
			syslog_server(LOG_ERR, "Grant route: Route found conflicts with others");
		} else if (strcmp(route_id, "not_clear") == 0) {
			syslog_server(LOG_ERR, "Grant route: Route found has occupied tracks");
		} else {
			syslog_server(LOG_ERR, "Grant route: Route could not be granted");
		}
	}

	pthread_mutex_unlock(&interlocker_mutex);
	return route_id;
}

void release_route(const int route_id) {
	pthread_mutex_lock(&interlocker_mutex);
	t_interlocking_route *route = get_route(route_id);
	if (route->train != NULL) {
        free(route->train);
        route->train = NULL;
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

onion_connection_status handler_set_interlocker(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	
	return OCS_NOT_IMPLEMENTED;
}

onion_connection_status handler_unset_interlocker(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	
	return OCS_NOT_IMPLEMENTED;
}
