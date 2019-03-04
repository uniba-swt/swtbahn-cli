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
#include "param_verification.h"
#include "interlocking.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool route_is_blocked_or_conflicted(const int route_id) {
	// Check blocked status
	if (interlocking_table_ultraloop[route_id].train_id != NULL) {
		return true;
	}
	
	// Check conflicts
	size_t conflicts_count = interlocking_table_ultraloop[route_id].conflicts_count;
	for (size_t conflict_index = 0; conflict_index < conflicts_count; conflict_index++) {
		const size_t conflicted_route_id = 
		    interlocking_table_ultraloop[route_id].conflicts[conflict_index];
		if (interlocking_table_ultraloop[conflicted_route_id].train_id != NULL) {
			return true;
		}
	}
	
	return false;
}

static bool route_is_clear(const int route_id, const char *train_id) {
	if (route_id == -1) {
		syslog(LOG_ERR, "Route is clear: Route %d is invalid", route_id);
		return false;
	}
	
	// Read the track state
	t_bidib_track_state track_state = bidib_get_state();
	
	// Signal at the route source has to be red (stop aspect)
	const size_t source_signal_state_index = 
	    interlocking_table_ultraloop[route_id].source.bidib_state_index;
	if (strcmp(track_state.signals_board[source_signal_state_index].data.state_id, "red")) {
		syslog(LOG_ERR, "Route is clear: Route %d - source signal is not in the stop aspect", 
		       route_id);
		bidib_free_track_state(track_state);
		return false;
	}
	
	// Signals of tracks that intersect the route have to be red (stop aspect)
	size_t signals_count = interlocking_table_ultraloop[route_id].signals_count;
	for (size_t signal_index = 0; signal_index < signals_count; signal_index++) {
		const char *signal_id = 
		    interlocking_table_ultraloop[route_id].signals[signal_index].id;
		const size_t signal_state_index = 
		    interlocking_table_ultraloop[route_id].signals[signal_index].bidib_state_index;
		if (strcmp(track_state.signals_board[signal_state_index].data.state_id, "red")) {
			syslog(LOG_ERR, "Route is clear: Route %d - signal %s is not in the stop aspect", 
			       route_id, signal_id);
			bidib_free_track_state(track_state);
			return false;
		}
	}

	// All track segments on the route have to be clear
	t_bidib_id_query train_id_query;
	size_t path_count = interlocking_table_ultraloop[route_id].path_count;
	for (size_t segment_index = 0; segment_index < path_count; segment_index++) {
		const char *segment_id = 
		    interlocking_table_ultraloop[route_id].path[segment_index].id;
		const size_t segment_state_index = 
		    interlocking_table_ultraloop[route_id].path[segment_index].bidib_state_index;
		
		if (track_state.segments[segment_state_index].data.occupied) {
			// Only the first track segment can be occupied, and only by the requesting train
			if (segment_index == 0 && 
				    track_state.segments[segment_state_index].data.dcc_address_cnt == 1) {
				train_id_query = 
				    bidib_get_train_id(track_state.segments[segment_state_index].data.dcc_addresses[0]);
				if (strcmp(train_id, train_id_query.id)) {
					syslog(LOG_ERR, "Route is clear: Route %d - track segment %s has not been cleared", 
					       route_id, segment_id);
					bidib_free_id_query(train_id_query);
					bidib_free_track_state(track_state);
					return false;
				}
			} else {
				syslog(LOG_ERR, "Route is clear: Route %d - track segment %s has not been cleared", 
				       route_id, segment_id);
				bidib_free_id_query(train_id_query);
				bidib_free_track_state(track_state);
				return false;
			}
		}
	}
	
	bidib_free_id_query(train_id_query);
	bidib_free_track_state(track_state);
	return true;
}

static bool set_route_points_signals(const int route_id) {
	// Set points
	size_t points_count = interlocking_table_ultraloop[route_id].points_count;
	for (size_t point_index = 0; point_index < points_count; point_index++) {
		const char *point_id = 
		    interlocking_table_ultraloop[route_id].points[point_index].id;
		const e_interlocking_point_position point_position = 
		    interlocking_table_ultraloop[route_id].points[point_index].position;
		
		if (bidib_switch_point(point_id, (point_position == NORMAL) ? "normal" : "reverse")) {
			syslog(LOG_ERR, "Execute route: Set point - invalid parameters");
			return false;
		} else {
			syslog(LOG_NOTICE, "Execute route: Set point - point: %s state: %s",
				   point_id, 
				   (point_position == NORMAL) ? "normal" : "reverse");
			bidib_flush();
		}
	}
	
	// Set entry signal to green (proceed aspect)
	const char *signal_id = interlocking_table_ultraloop[route_id].source.id;
	if (bidib_set_signal(signal_id, "green")) {
		syslog(LOG_ERR, "Execute route: Set signal - invalid parameters");
		return false;
	} else {
		syslog(LOG_NOTICE, "Execute route: Set signal - signal: %s state: %s",
			   signal_id, "green");
		bidib_flush();
	}

	return true;
}

int grant_route(const char *train_id, const char *source_id, const char *destination_id) {
	pthread_mutex_lock(&interlocker_mutex);

	// Get a route from the interlocking table
	int route_id = interlocking_table_get_route_id(source_id, destination_id);
	
	// Check if the route is blocked or conflicted
	if (route_id == -1) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog(LOG_ERR, "Grant route: No route found from %s to %s", source_id, destination_id);
		return -1;
	}
	if (route_is_blocked_or_conflicted(route_id)) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog(LOG_ERR, "Grant route: Route %d is blocked or has conflicts", route_id);
		return -1;
	}
	
	printf("Route ID: %d\n", route_id);
	
	// Check if route has been cleared
	if (!route_is_clear(route_id, train_id)) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog(LOG_ERR, "Grant route: Route %d has not been cleared", route_id);
		return -1;
	}
	
	interlocking_table_ultraloop[route_id].train_id = g_string_new(train_id);
	pthread_mutex_unlock(&interlocker_mutex);
	
	// Set the route points and signals
	if (!set_route_points_signals(route_id)) {
		syslog(LOG_ERR, "Grant route: Route %d could not be set", route_id);
		return -1;
	}
	
	// Return the ID of the granted route
	syslog(LOG_NOTICE, "Grant route: Release %d has been granted", route_id);
	return route_id;
}

onion_connection_status handler_release_route(void *_, onion_request *req,
                                          onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route");
		const int route_id = params_check_route_id(data_route_id);
		if (route_id == -1) {
			syslog(LOG_ERR, "Request: Release route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			pthread_mutex_lock(&interlocker_mutex);
			g_string_free(interlocking_table_ultraloop[route_id].train_id, TRUE);
			interlocking_table_ultraloop[route_id].train_id = NULL;
			pthread_mutex_unlock(&interlocker_mutex);
			
			// Set entry signal to red (stop aspect)
			const char *signal_id = interlocking_table_ultraloop[route_id].source.id;
			if (bidib_set_signal(signal_id, "red")) {
				syslog(LOG_ERR, "Request: Release route - Entry signal not set to stop aspect");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog(LOG_NOTICE, "Request: Release route: Set signal - signal: %s state: %s",
					   signal_id, "red");
				bidib_flush();
				return OCS_PROCESSED;
			}
			
			syslog(LOG_NOTICE, "Request: Release route - route: %d", route_id);
			return OCS_PROCESSED;
		}
	} else {
		syslog(LOG_ERR, "Request: Release route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_point == NULL || data_state == NULL) {
			syslog(LOG_ERR, "Request: Set point - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_switch_point(data_point, data_state)) {
				syslog(LOG_ERR, "Request: Set point - invalid parameters");
				bidib_flush();
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog(LOG_NOTICE, "Request: Set point - point: %s state: %s",
				       data_point, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog(LOG_ERR, "Request: Set point - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_signal = onion_request_get_post(req, "signal");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_signal == NULL || data_state == NULL) {
			syslog(LOG_ERR, "Request: Set signal - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_set_signal(data_signal, data_state)) {
				syslog(LOG_ERR, "Request: Set signal - invalid parameters");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog(LOG_NOTICE, "Request: Set signal - signal: %s state: %s",
				       data_signal, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog(LOG_ERR, "Request: Set signal - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

