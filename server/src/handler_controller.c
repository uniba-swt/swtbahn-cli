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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 *
 */

#include <bidib/bidib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "handler_controller.h"
#include "server.h"
#include "dyn_containers_interface.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"
#include "check_route_sectional/check_route_sectional_direct.h"
#include "json_response_builder.h"
#include "communication_utils.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MICROSECOND 1
static const int let_period_us = 100000 * MICROSECOND;	// 0.1 seconds

static t_interlocker_data interlocker_instances[INTERLOCKER_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_interlocker_instance = -1 }
};

static GString *selected_interlocker_name = NULL;
static int selected_interlocker_instance = -1;
static const size_t max_signals_in_route_assmptn = 1024;
static const size_t max_items_in_route_assmptn = 1024;


/**
 * @brief Set the currently selected interlocker to be the interlocker with 
 * name given by `interlocker_name interlocker_name`, if an interlocker with this name exists
 * and no interlocker is currently set.
 * Shall only be called with interlocker_mutex locked.
 * 
 * @param interlocker_name name of the interlocker to set, shall not be NULL
 * @return int >= 0 if interlocker was set successfully, otherwise -1.
 */
static int set_interlocker(const char *interlocker_name) {
	if (interlocker_name == NULL) {
		syslog_server(LOG_ERR, "Set interlocker - invalid (NULL) interlocker_name");
		return -1;
	} else if (selected_interlocker_instance != -1) {
		// Another interlocker is already set, return -1
		syslog_server(LOG_ERR,
		              "Set interlocker - interlocker: %s - another interlocker is already set!",
		              interlocker_name);
		return -1;
	}
	
	for (int i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		// Look for not already used interlocker instance slot (indicated by is_valid being false).
		if (!interlocker_instances[i].is_valid) {
			if (dyn_containers_set_interlocker_instance(&interlocker_instances[i], interlocker_name)) {
				syslog_server(LOG_ERR, 
				              "Set interlocker - interlocker: %s - could not be used in instance %d",
				              interlocker_name, i);
				return -1;
			} else {
				selected_interlocker_name = g_string_new(interlocker_name);
				selected_interlocker_instance = i;
				interlocker_instances[selected_interlocker_instance].is_valid = true;
				return selected_interlocker_instance;
			}
		}
	}
	syslog_server(LOG_ERR,
	              "Set interlocker - interlocker: %s - no interlocker instance/slot available",
	              interlocker_name);
	return -1;
}

/**
 * @brief Un-set the interlocker given the name of the currently set interlocker.
 * Shall only be called with interlocker_mutex locked.
 * 
 * @param interlocker_name name of the interlocker to unset
 * @return int -1 if no interlocker is set after unsetting, >= 0 if an interlocker remains set, 
 * i.e., unsetting failed if the returned value is >= 0.
 */
static int unset_interlocker(const char *interlocker_name) {
	if (selected_interlocker_instance == -1) {
		// Selected interlocker instance is -1 -> no interlocker to unset.
		return -1;
	}
	
	if (strcmp(selected_interlocker_name->str, interlocker_name) == 0
			&& interlocker_instances[selected_interlocker_instance].is_valid) {
		dyn_containers_free_interlocker_instance(&interlocker_instances[selected_interlocker_instance]);
		interlocker_instances[selected_interlocker_instance].is_valid = false;
		g_string_free(selected_interlocker_name, true);
		selected_interlocker_name = NULL;
		selected_interlocker_instance = -1;
	}
	return selected_interlocker_instance;
}

bool load_default_interlocker_instance() {
	while (!dyn_containers_is_running()) {
		usleep(let_period_us);
	}
	pthread_mutex_lock(&interlocker_mutex);
	const int result = set_interlocker("libinterlocker_default (unremovable)");
	pthread_mutex_unlock(&interlocker_mutex);
	// return true if loading failed, otherwise false
	return (result == -1);
}

void release_all_interlockers(void) {
	pthread_mutex_lock(&interlocker_mutex);
	if (selected_interlocker_name != NULL) {
		g_string_free(selected_interlocker_name, true);
		selected_interlocker_name = NULL;
	}
	for (int i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		if (interlocker_instances[i].is_valid) {
			dyn_containers_free_interlocker_instance(&interlocker_instances[i]);
			interlocker_instances[i].is_valid = false;
		}
	}
	selected_interlocker_instance = -1;
	pthread_mutex_unlock(&interlocker_mutex);
}

/**
 * Check if a sectional interlocker is in use. 
 * Shall only be called with interlocker_mutex locked.
 * 
 * @return true if the currently used interlocker name contains "sectional", i.e., 
 * a sectional interlocker is in use.
 * @return false otherwise
 */
static bool is_sectional_interlocker_in_use() {
	return selected_interlocker_name != NULL 
	       && (g_strrstr(selected_interlocker_name->str, "sectional") != NULL);
}

bool get_route_has_granted_conflicts(const char *route_id) {
	if (route_id == NULL) {
		return false;
	}
	// When a sectional interlocker is in use, use the sectional checker to check for conflicts.
	const bool sectional_in_use = is_sectional_interlocker_in_use();
	
	// Amount of conflicting routes can't be larger than overall amount of routes known.
	const unsigned int route_count = interlocking_table_get_size();
	char *conflict_routes[route_count];
	const int conflict_routes_len = 
			config_get_array_string_value("route", route_id, "conflicts", conflict_routes);
	
	for (int i = 0; i < conflict_routes_len; i++) {
		const t_interlocking_route *conflict_route = get_route(conflict_routes[i]);
		if (conflict_route != NULL && conflict_route->train != NULL) {
			if (sectional_in_use && is_route_conflict_safe_sectional(conflict_routes[i], route_id)) {
				// If a sectional checker is in use and it says that this conflict is 
				// actually "safe", skip this conflict.
				continue;
			} else {
				return true;
			}
		}
	}
	return false;
}

GArray *get_granted_route_conflicts(const char *route_id, bool include_conflict_train_info) {
	if (route_id == NULL) {
		return NULL;
	}
	GArray* conflict_route_ids = g_array_new(FALSE, FALSE, sizeof(char *));
	
	// When a sectional interlocker is in use, use the sectional checker to
	// check for route availability.
	const bool sectional_in_use = is_sectional_interlocker_in_use();
	
	// For the route with id=route_id, get all conflicting routes and add them to the
	// GArray that will be returned if they are granted.
	
	// Amount of conflicting routes can't be larger than overall amount of routes known.
	const unsigned int route_count = interlocking_table_get_size();
	char *conflict_routes[route_count];
	const int conflict_routes_len = 
			config_get_array_string_value("route", route_id, "conflicts", conflict_routes);
	for (int i = 0; i < conflict_routes_len; i++) {
		const t_interlocking_route *conflict_route = get_route(conflict_routes[i]);
		if (conflict_route != NULL && conflict_route->train != NULL) {
			if (sectional_in_use && is_route_conflict_safe_sectional(conflict_routes[i], route_id)) {
				// If a sectional checker is in use and it says that this conflict is 
				// actually "safe" to grant, skip this conflict/dont add it to the list.
				continue;
			}
			size_t conflict_entry_len = strlen(conflict_route->id) + 1;
			if (!include_conflict_train_info) {
				conflict_entry_len += strlen(conflict_route->train) + 3;
			}
			char *conflict_route_id_string = malloc(sizeof(char) * conflict_entry_len);
			if (conflict_route_id_string == NULL) {
				syslog_server(LOG_ERR, 
				              "get_granted_route_conflicts - failed to allocate memory"
				              " for conflict_route_id_string");
				for (unsigned int n = 0; n < conflict_route_ids->len; ++n) {
					char *elem = g_array_index(conflict_route_ids, char *, n);
					free(elem);
				}
				g_array_free(conflict_route_ids, true);
				return NULL;
			}
			if (include_conflict_train_info) {
				snprintf(conflict_route_id_string, conflict_entry_len, "%s (%s)",
				         conflict_route->id, conflict_route->train);
			} else {
				snprintf(conflict_route_id_string, conflict_entry_len, "%s", conflict_route->id);
			}
			g_array_append_val(conflict_route_ids, conflict_route_id_string);
		}
	}
	return conflict_route_ids;
}

bool get_route_is_clear(const char *route_id) {
	if (route_id == NULL) {
		return false;
	}
	bahn_data_util_init_cached_track_state();
	
	// Check that all route signals are in the Stop aspect
	char *signal_ids[max_signals_in_route_assmptn];
	const int signal_ids_len = 
			config_get_array_string_value("route", route_id, "route_signals", signal_ids);
	for (int i = 0; i < signal_ids_len; i++) {
		const char *signal_state = track_state_get_value(signal_ids[i]);
		if (strcmp(signal_state, "stop")) {
			bahn_data_util_free_cached_track_state();
			return false;
		}
	}
	
	// Check that all segments are unoccupied
	char *item_ids[max_items_in_route_assmptn];
	const int item_ids_len = config_get_array_string_value("route", route_id, "path", item_ids);
	for (int i = 0; i < item_ids_len; i++) {
		if (is_type_segment(item_ids[i]) && is_segment_occupied(item_ids[i])) {
			bahn_data_util_free_cached_track_state();
			return false;
		}
	}
	
	bahn_data_util_free_cached_track_state();
	return true;
}

GString *grant_route(const char *train_id, const char *source_id, const char *destination_id) {
	if (train_id == NULL || source_id == NULL || destination_id == NULL) {
		syslog_server(LOG_ERR, "Grant route - invalid (NULL) parameters");
		return g_string_new("not_grantable");
	}
	
	pthread_mutex_lock(&interlocker_mutex);
	if (selected_interlocker_instance == -1) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_ERR, 
		              "Grant route - train: %s from: %s to: %s - no interlocker has been set",
		              train_id, source_id, destination_id);
		return g_string_new("no_interlocker");
	}
	
	bahn_data_util_init_cached_track_state();
	
	// Ask the interlocker to grant requested route.
	// May take multiple ticks to process the request.
	dyn_containers_set_interlocker_instance_inputs(&interlocker_instances[selected_interlocker_instance],
	                                               source_id, destination_id,
	                                               train_id);
	
	struct t_interlocker_instance_io interlocker_instance_io;
	do {
		usleep(let_period_us);
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[selected_interlocker_instance],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_has_reset);
	
	dyn_containers_set_interlocker_instance_reset(&interlocker_instances[selected_interlocker_instance],
	                                              false);
	
	do {
		usleep(let_period_us);
		dyn_containers_get_interlocker_instance_outputs(&interlocker_instances[selected_interlocker_instance],
		                                                &interlocker_instance_io);
	} while (!interlocker_instance_io.output_terminated);
	
	// Return the result
	GString *g_route_id_copy = g_string_new(interlocker_instance_io.output_route_id);
	bahn_data_util_free_cached_track_state();
	pthread_mutex_unlock(&interlocker_mutex);
	
	if (g_route_id_copy->str != NULL && params_check_is_number(g_route_id_copy->str)) {
		syslog_server(LOG_NOTICE, 
		              "Grant route - train: %s from: %s to: %s - route %s has been granted", 
		              train_id, source_id, destination_id, g_route_id_copy->str);
	} else if (strcmp(g_route_id_copy->str, "no_routes") == 0) {
		syslog_server(LOG_WARNING, 
		              "Grant route - train: %s from: %s to: %s - no route possible",
		              train_id, source_id, destination_id);
	} else if (strcmp(g_route_id_copy->str, "not_grantable") == 0) {
		syslog_server(LOG_WARNING, 
		              "Grant route - train: %s from: %s to: %s - conflicting routes in use",
		              train_id, source_id, destination_id);
	} else if (strcmp(g_route_id_copy->str, "not_clear") == 0) {
		syslog_server(LOG_WARNING, 
		              "Grant route - train: %s from: %s to: %s - route blocked or source signal not stop",
		              train_id, source_id, destination_id);
	} else {
		syslog_server(LOG_WARNING, 
		              "Grant route - train: %s from: %s to: %s - route could not be granted (message: %s)",
		              train_id, source_id, destination_id, g_route_id_copy->str);
	}
	return g_route_id_copy;
}

const char *grant_route_id(const char *train_id, const char *route_id) {
	if (train_id == NULL || route_id == NULL) {
		syslog_server(LOG_ERR, "Grant route id - invalid (NULL) parameters");
		return "not_grantable";
	}
	pthread_mutex_lock(&interlocker_mutex);
	// Check whether the route can be granted
	t_interlocking_route *route = get_route(route_id);
	if (route == NULL) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_ERR, "Grant route id - unknown route id %s", route_id);
		return "not_known";
	} else if (route->train != NULL) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_WARNING, 
		              "Grant route id - route: %s train: %s - route already granted",
		              route_id, train_id);
		return "already_granted";
	} else if (get_route_has_granted_conflicts(route_id)) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_WARNING, 
		              "Grant route id - route: %s train: %s - conflicting routes are in use",
		              route_id, train_id);
		return "not_grantable";
	} else if (!get_route_is_clear(route_id)) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_WARNING, 
		              "Grant route id - route: %s train: %s - route is not clear",
		              route_id, train_id);
		return "not_clear";
	}
	
	// Grant the route to the train
	
	syslog_server(LOG_INFO, 
	              "Grant route id - route: %s train: %s - checks passed, now grant route", 
	              route_id, train_id);
	
	route->train = strdup(train_id);
	
	if (route->train == NULL) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_ERR, 
		              "Grant route id - route: %s train: %s - "
		              "unable to allocate memory for route->train",
		              route_id, train_id);
		return "internal_error";
	}
	
	// Set the points to their required positions
	for (unsigned int i = 0; i < route->points->len; i++) {
		const t_interlocking_point point = g_array_index(route->points, t_interlocking_point, i);
		const char *position = (point.position == NORMAL) ? "normal" : "reverse";
		bidib_switch_point(point.id, position);
		bidib_flush();
	}
	
	/// TODO: Discuss - at this point we could add a wait of ~2s and then check if the points are
	///       in their required position. 
	///       Benefit: Detect hardware failures, thus preventing a potential short circuit later.
	///       Drawback: Latency increases.
	
	// Set the signals to their required aspects
	for (unsigned int i = 0; i < route->signals->len - 1; i++) {
		const char *signal = g_array_index(route->signals, char *, i);
		const char *signal_type = config_get_scalar_string_value("signal", signal, "type");
		const char *signal_aspect = 
				strcmp(signal_type, "shunting") == 0 ? "aspect_shunt" : "aspect_go";
		bidib_set_signal(signal, signal_aspect);
		bidib_flush();
	}
	
	pthread_mutex_unlock(&interlocker_mutex);
	
	syslog_server(LOG_NOTICE, 
	              "Grant route id - route: %s train: %s - route granted", 
	              route_id, train_id);
	return "granted";
}

///TODO: This should not unconditionally set all route signals to stop, because that would
//       prevent sectional route release from working correctly
static bool release_route_intern(t_interlocking_route *route) {
	// Shall only be called with interlocker_mutex locked.
	if (route == NULL) {
		syslog_server(LOG_ERR, "Release route - invalid (NULL) parameters");
		return false;
	} else if (route->train != NULL) {
		syslog_server(LOG_INFO, 
		              "Release route - route: %s - currently granted to train %s, "
		              "now setting all route signals to aspect_stop", 
		              route->id, route->train);
		const char *signal_aspect = "aspect_stop";
		for (int signal_index = 0; signal_index < route->signals->len; signal_index++) {
			const char *signal_id = g_array_index(route->signals, char *, signal_index);
			
			if (bidib_set_signal(signal_id, signal_aspect)) {
				syslog_server(LOG_ERR, 
				              "Release route - route: %s - unable to set signal to aspect %s", 
				              route->id, signal_aspect);
			} else {
				bidib_flush();
			}
		}
		// Ref to be able to set route->train to NULL (effectively releasing the route from the train)
		// and still release the train id string after logging.
		char *train_id_route_ptr = route->train;
		route->train = NULL;
		syslog_server(LOG_NOTICE, 
		              "Release route - route: %s - released (from train %s)", 
		              route->id, train_id_route_ptr);
		free(train_id_route_ptr);
		return true;
	} else {
		syslog_server(LOG_ERR, "Release route - route: %s - is not granted to any train", route->id);
		return false;
	}
}

bool release_route(const char *route_id) {
	if (route_id == NULL) {
		syslog_server(LOG_ERR, "Release route - invalid (NULL) route_id");
		return false;
	}
	pthread_mutex_lock(&interlocker_mutex);
	t_interlocking_route *route = get_route(route_id);
	bool ret = release_route_intern(route);
	pthread_mutex_unlock(&interlocker_mutex);
	return ret;
}

void release_all_granted_routes() {
	pthread_mutex_lock(&interlocker_mutex);
	GArray *route_ids = interlocking_table_get_all_route_ids_shallowcpy();
	if (route_ids != NULL) {
		for (unsigned int i = 0; i < route_ids->len; i++) {
			const char *route_id = g_array_index(route_ids, char *, i);
			if (route_id != NULL) {
				t_interlocking_route *route = get_route(route_id);
				if (route != NULL && route->train != NULL) {
					release_route_intern(route);
				}
			}
		}
	}
	// free the GArray but not the contained strings, as it was created by shallow copy.
	g_array_free(route_ids, true);
	pthread_mutex_unlock(&interlocker_mutex);
}

bool reversers_state_update(void) {
	const int max_retries = 5;
	bool error = false;
	
	t_bidib_id_list_query rev_query = bidib_get_connected_reversers();
	for (size_t i = 0; i < rev_query.length; i++) {
		const char *reverser_id = rev_query.ids[i];
		const char *reverser_board =
				config_get_scalar_string_value("reverser", reverser_id, "board");
		error |= bidib_request_reverser_state(reverser_id, reverser_board);
		bidib_flush();
		
		bool state_unknown = true;
		for (int retry = 0; retry < max_retries && state_unknown; retry++) {
			t_bidib_reverser_state_query rev_state_query = bidib_get_reverser_state(reverser_id);
			if (rev_state_query.available) {
				state_unknown = (rev_state_query.data.state_value == BIDIB_REV_EXEC_STATE_UNKNOWN);
			}
			bidib_free_reverser_state_query(rev_state_query);
			if (!state_unknown) {
				break;
			}
			
			usleep(50000);   // 0.05s
		}
		
		error |= state_unknown;
	}
	
	bidib_free_id_list_query(rev_query);
	return !error;
}

o_con_status handler_release_route(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		
		if (handle_param_miss_check(res, "Release route", "route-id", data_route_id)) {
			return OCS_PROCESSED;
		} else if (strcmp(route_id, "") == 0) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid route-id");
			// log the original input (data_route_id), not the parsed (route_id), for debugging
			syslog_server(LOG_ERR, "Request: Release route - invalid route-id (%s)", data_route_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Release route - route: %s - start", route_id);
		bool release_success = release_route(route_id);
		if (release_success) {
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, "Request: Release route - route: %s - finish", route_id);
		} else {
			send_common_feedback(res, HTTP_BAD_REQUEST, 
			                     "invalid route-id, route does not exist or is not granted");
			syslog_server(LOG_ERR, "Request: Release route - route: %s - abort", route_id);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Release route");
	}
}

o_con_status handler_set_point(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		const char *data_state = onion_request_get_post(req, "state");
		
		if (handle_param_miss_check(res, "Set point", "point", data_point)
			|| handle_param_miss_check(res, "Set point", "state", data_state)) {
			return OCS_PROCESSED;
		} else if (!is_type_point(data_point)) {
			send_common_feedback(res, HTTP_NOT_FOUND, "unknown point");
			syslog_server(LOG_ERR, "Request: Set point - unknown point (%s)", data_point);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set point - point: %s state: %s - start",
		              data_point, data_state);
		if (bidib_switch_point(data_point, data_state)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set point - point: %s state: %s - "
			              "invalid parameter values - abort",
			              data_point, data_state);
		} else {
			bidib_flush();
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, 
			              "Request: Set point - point: %s state: %s - finish",
			              data_point, data_state);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set point");
	}
}

o_con_status handler_set_signal(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_signal = onion_request_get_post(req, "signal");
		const char *data_state = onion_request_get_post(req, "state");
		
		if (handle_param_miss_check(res, "Set signal", "signal", data_signal)
			|| handle_param_miss_check(res, "Set signal", "state", data_state)) {
			return OCS_PROCESSED;
		} else if (!is_type_signal(data_signal)) {
			send_common_feedback(res, HTTP_NOT_FOUND, "unknown signal");
			syslog_server(LOG_ERR, "Request: Set point - unknown signal (%s)", data_signal);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set signal - signal: %s state: %s - start",
		              data_signal, data_state);
		if (bidib_set_signal(data_signal, data_state)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set signal - signal: %s state: %s - "
			              "invalid parameter values - abort", 
			              data_signal, data_state);
		} else {
			bidib_flush();
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, 
			              "Request: Set signal - signal: %s state: %s - finish",
			              data_signal, data_state);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set signal");
	}
}

o_con_status handler_set_peripheral(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_peripheral = onion_request_get_post(req, "peripheral");
		const char *data_state = onion_request_get_post(req, "state");
		
		if (handle_param_miss_check(res, "Set peripheral", "peripheral", data_peripheral)
			|| handle_param_miss_check(res, "Set peripheral", "state", data_state)) {
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set peripheral - peripheral: %s state: %s - start",
		              data_peripheral, data_state);
		if (bidib_set_peripheral(data_peripheral, data_state)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set peripheral - peripheral: %s state: %s - "
			              "invalid parameter values - abort", 
			              data_peripheral, data_state);
		} else {
			bidib_flush();
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, 
			              "Request: Set peripheral - peripheral: %s state: %s - finish", 
			              data_peripheral, data_state);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set peripheral");
	}
}

o_con_status handler_get_interlocker(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		pthread_mutex_lock(&interlocker_mutex);
		if (selected_interlocker_instance != -1 && selected_interlocker_name != NULL) {
			GString *g_resstr = g_string_new("{\"interlocker\": \"");
			g_string_append_printf(g_resstr, "%s\"}", 
			                       selected_interlocker_name->str);
			pthread_mutex_unlock(&interlocker_mutex);
			send_some_gstring_and_free(res, HTTP_OK, g_resstr);
			syslog_server(LOG_INFO, "Request: Get interlocker - done");
		} else {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, HTTP_NOT_FOUND, "No interlocker is currently selected");
			syslog_server(LOG_ERR, "Request: Get interlocker - none selected");
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get interlocker");
	}
}

o_con_status handler_set_interlocker(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_interlocker = onion_request_get_post(req, "interlocker");
		
		if (handle_param_miss_check(res, "Set interlocker", "interlocker", data_interlocker)) {
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set interlocker - interlocker: %s - start",
		              data_interlocker);
		pthread_mutex_lock(&interlocker_mutex);
		if (selected_interlocker_instance != -1) {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "another interlocker instance is already set");
			syslog_server(LOG_ERR, 
			              "Request: Set interlocker - interlocker: %s - another "
			              "interlocker instance is already set - abort", 
			              data_interlocker);
		} else if (set_interlocker(data_interlocker) == -1) {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid interlocker name "
			                     "or no more interlocker instances can be loaded");
			syslog_server(LOG_ERR, 
			              "Request: Set interlocker - interlocker: %s - invalid interlocker "
			              "name or no more interlocker instances can be loaded - abort", 
			              data_interlocker);
		} else {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, 
			              "Request: Set interlocker - interlocker: %s - finish",
			              data_interlocker);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set interlocker");
	}
}

o_con_status handler_unset_interlocker(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_interlocker = onion_request_get_post(req, "interlocker");
		
		if (handle_param_miss_check(res, "Unset interlocker", "interlocker", data_interlocker)) {
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Unset interlocker - interlocker: %s - start",
		              data_interlocker);
		pthread_mutex_lock(&interlocker_mutex);
		if (selected_interlocker_instance == -1) {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "no interlocker instance is set that can be unset");
			syslog_server(LOG_ERR, 
			              "Request: Unset interlocker - interlocker: %s - "
			              "no interlocker instance to unset - abort", 
			              data_interlocker);
		} else if (unset_interlocker(data_interlocker) != -1) {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "invalid interlocker name (curr"
			                     "ently set interlocker doesn't match provided interlocker name)");
			syslog_server(LOG_ERR, 
			              "Request: Unset interlocker - interlocker: %s - "
			              "invalid interlocker name - abort", 
			              data_interlocker);
		} else {
			pthread_mutex_unlock(&interlocker_mutex);
			send_common_feedback(res, HTTP_OK, "");
			syslog_server(LOG_NOTICE, 
			              "Request: Unset interlocker - interlocker: %s - finish",
			              data_interlocker);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Unset interlocker");
	}
}
