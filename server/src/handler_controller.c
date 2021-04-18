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
#include <string.h>

#include "server.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"

#include "interlocking_algorithm.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

bool route_is_unavailable_or_conflicted(const int route_id) {
	t_interlocking_route *route = get_route(route_id);

	// Check if the route has been granted (unavailable)
	if (route->train != NULL) {
		syslog_server(LOG_ERR, "Route has already been granted: %d", route_id);
		return true;
	} else {
		syslog_server(LOG_NOTICE, "Route available: %d", route_id);
	}
	
	// Check conflicts
	if (route->conflicts != NULL) {
		for (int i = 0; i < route->conflicts->len; ++i) {
			char *conflicted_route_id = g_array_index(route->conflicts, char *, i);
			if (get_route_str(conflicted_route_id)->train != NULL) {
				syslog_server(LOG_ERR, "Conflicting route has been granted: %d", conflicted_route_id);
				return true;
			}
		}
	}

	return false;
}

bool route_is_clear(const int route_id, const char *train_id) {
	if (route_id == -1) {
		syslog_server(LOG_ERR, "Route is clear: Route %d is invalid", route_id);
		return false;
	} else {
		syslog_server(LOG_NOTICE, "Route is clear: Route %d is valid for train %s", route_id, train_id);
	}

	t_interlocking_route *route = get_route(route_id);
	// Signal at the route source has to be red (stop aspect)
	if (!string_equals(track_state_get_value(route->source), "stop")) {
		syslog_server(LOG_ERR, "Route is clear: Route %d - source signal is not in the stop aspect", route_id);
		return false;
	}
	
	// Signals of tracks that intersect the route have to be red (stop aspect)
	if (route->signals != NULL) {
		for (int signal_index = 0; signal_index < route->signals->len; ++signal_index) {
            char *signal_id = g_array_index(route->signals, char *, signal_index);
            if (!string_equals(signal_id, "stop")) {
				syslog_server(LOG_ERR, "Route is clear: Route %d - signal %s is not in the stop aspect",
				              route_id, signal_id);
				return false;
			}
		}
	}

	// All track segments on the route have to be clear
	for (int segment_index = 0; segment_index < route->path->len; ++segment_index) {
        char * segment_id = g_array_index(route->path, char *, segment_index);
		if (is_segment_occupied(segment_id)) {
			syslog_server(LOG_ERR, "Route is clear: Route %d - track segment %s has not been cleared",
			              route_id, segment_id);
			return false;
		}
	}

	return true;
}

bool set_route_points_signals(const int route_id) {
	syslog_server(LOG_NOTICE, "set_route_points_signals: route_id = %d", route_id);
	              t_interlocking_route *route = get_route(route_id);

	// Set points
	if (route->points != NULL) {
		for (int point_index = 0; point_index < route->points->len; ++point_index) {
			t_interlocking_point point = g_array_index(route->points, t_interlocking_point, point_index);
			const char *point_id = point.id;
			char *pos = config_get_point_position(route->id, point_id);
            if (track_state_set_value(point_id, pos)) {
				syslog_server(LOG_ERR, "Execute route: Set point - invalid parameters");
				return false;
			} else {
				syslog_server(LOG_NOTICE, "Execute route: Set point - point: %s state: %s", point_id, pos);
				bidib_flush();
			}
		}
	}
	
	// Set entry signal to green (proceed aspect)
	if (track_state_set_value(route->source, "clear")) {
		syslog_server(LOG_ERR, "Execute route: Set signal - invalid parameters");
		return false;
	} else {
		syslog_server(LOG_NOTICE, "Execute route: Set signal - signal: %s state: %s", route->source, "green");
		bidib_flush();
	}

	return true;
}

bool block_route(const int route_id, const char *train_id) {
	syslog_server(LOG_NOTICE, "Block route: train %s on route %d", train_id, route_id);
	t_interlocking_route *route = get_route(route_id);
	route->train = strdup(train_id);
	return true;
}

int grant_route_with_algorithm(const char *train_id, const char *source_id, const char *destination_id) {
	t_interlocking_algorithm_tick_data interlocking_data;
	interlocking_algorithm_reset(&interlocking_data);

	// Set the inputs
	interlocking_data.request_available = true;
	interlocking_data.train_id = train_id;
	interlocking_data.source_id = source_id;
	interlocking_data.destination_id = destination_id;
	
	// Execute the algorithm
	pthread_mutex_lock(&interlocker_mutex);
    init_cached_track_state();
	interlocking_algorithm_tick(&interlocking_data);
    free_cached_track_state();
	pthread_mutex_unlock(&interlocker_mutex);

	interlocking_data.request_available = false;

	switch (interlocking_data.route_id) {
		case (-1):
			syslog_server(LOG_ERR, "Grant route with algorithm: Route could not be granted (1. Wait)");
			return -1;
		case (-2):
			syslog_server(LOG_ERR, "Grant route with algorithm: Route could not be granted (2. Find)");
			return -1;
		case (-3):
			syslog_server(LOG_ERR, "Grant route with algorithm: Route could not be granted (3. Grantable)");
			return -1;
		case (-4):
			syslog_server(LOG_ERR, "Grant route with algorithm: Route could not be granted (3. Clearance)");
			return -1;
		default:
			break;
	} 
	
	// Return the ID of the granted route
	syslog_server(LOG_NOTICE, "Grant route with algorithm: Route %d has been granted", interlocking_data.route_id);
	return interlocking_data.route_id;
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

