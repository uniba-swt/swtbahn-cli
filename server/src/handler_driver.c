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
#include <glib.h>
#include <string.h>

#include "server.h"
#include "dyn_containers_interface.h"
#include "handler_controller.h"
#include "interlocking.h"
#include "param_verification.h"
#include "bahn_data_util.h"

#define MICROSECOND 1
#define TRAIN_DRIVE_TIME_STEP 	10000 * MICROSECOND		// 0.01 seconds

pthread_mutex_t grabbed_trains_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int DRIVING_SPEED_SLOW = 50;
static unsigned int next_grab_id = 0;


typedef struct t_route_signal_info_s{
	char *id;
	bool has_been_set_to_stop;
	bool is_source_signal;
	bool is_destination_signal;
	size_t index_in_route_path;
	int number_of_occurrence;
} t_route_signal_info;

t_train_data grabbed_trains[TRAIN_ENGINE_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_engine_instance = -1 }
};


static void increment_next_grab_id(void) {
	if (next_grab_id == TRAIN_ENGINE_INSTANCE_COUNT_MAX - 1) {
		next_grab_id = 0;
	} else {
		next_grab_id++;
	}
}

const int train_get_grab_id(const char *train) {
	pthread_mutex_lock(&grabbed_trains_mutex);
	int grab_id = -1;
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		if (grabbed_trains[i].is_valid 
				&& strcmp(grabbed_trains[i].name->str, train) == 0) {
			grab_id = i;
			break;
		}
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	return grab_id;
}

bool train_grabbed(const char *train) {
	bool grabbed = false;
	pthread_mutex_lock(&grabbed_trains_mutex);
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		if (grabbed_trains[i].is_valid 
				&& grabbed_trains[i].name != NULL 
				&& strcmp(grabbed_trains[i].name->str, train) == 0) {
			grabbed = true;
			break;
		}
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	return grabbed;
}

static bool train_is_on_tracks(const char *train_id) {
	t_bidib_train_position_query train_position_query = bidib_get_train_position(train_id);
	bool ret = train_position_query.length > 0;
	bidib_free_train_position_query(train_position_query);
	return ret;
}

static bool train_position_is_at(const char *train_id, const char *segment) {
	t_bidib_train_position_query train_position_query = bidib_get_train_position(train_id);

	for (size_t i = 0; i < train_position_query.length; i++) {
		if (strcmp(segment, train_position_query.segments[i]) == 0) {
			bidib_free_train_position_query(train_position_query);
			return true;
		}
	}
	
	bidib_free_train_position_query(train_position_query);
	return false;
}

static const bool is_forward_driving(const t_interlocking_route *route, 
                                     const char *train_id) {

	t_bidib_train_position_query train_position_query = bidib_get_train_position(train_id);
	const bool train_is_left = train_position_query.orientation_is_left;
	const bool route_is_clockwise = (strcmp(route->orientation, "clockwise") == 0);
	const bool is_forwards = (route_is_clockwise && train_is_left)
	                         || (!route_is_clockwise && !train_is_left);
	                                
	// Determine whether the train is on a block controlled by a Kehrschleifenmodul
	// 1. Get train block
	char *block_id = NULL;
	for (size_t i = 0; i < train_position_query.length; i++) {
		block_id = config_get_block_id_of_segment(train_position_query.segments[i]);
		if (block_id != NULL) {
			break;
		}
	}
	bidib_free_train_position_query(train_position_query);
	
	if (block_id == NULL) {
		syslog_server(LOG_ERR, "Driving is forwards: %s - current block of train: %s is unknown",
					  is_forwards ? "yes" : "no", train_id);
		return is_forwards;
	}

	// 2. Check whether the train is on a block controlled by a reverser
	bool electrically_reversed = false;
	t_bidib_id_list_query rev_query = bidib_get_connected_reversers();
	for (size_t i = 0; i < rev_query.length; i++) {
		const char *reverser_id = rev_query.ids[i];
		const char *reverser_block = 
				config_get_scalar_string_value("reverser", reverser_id, "block");

		if (strcmp(block_id, reverser_block) == 0) {
			const bool succ = reversers_state_update();
			t_bidib_reverser_state_query rev_state_query =
					bidib_get_reverser_state(reverser_id);
			// 3. Check the reverser's state
			if (succ && rev_state_query.available) {
				electrically_reversed = 
						(rev_state_query.data.state_value == BIDIB_REV_EXEC_STATE_ON);
			}
			break;
		}
	}
	bidib_free_id_list_query(rev_query);
	
	const bool requested_forwards = electrically_reversed
	                                ? !is_forwards
	                                : is_forwards;
	syslog_server(LOG_NOTICE, "Driving is forwards: %s",
				  requested_forwards ? "yes" : "no");
	return requested_forwards;
}

static bool drive_route_params_valid(const char *train_id, t_interlocking_route *route) {
	if ((route->train == NULL) || strcmp(train_id, route->train) != 0) {
		syslog_server(LOG_NOTICE, "Check drive route params: Route %s not granted to train %s", 
		              route->id, train_id);
		return false;
	}
	return true;
}

static bool validate_interlocking_route_members_not_null(const t_interlocking_route const *route) {
	if (route == NULL) {
		return false;
	}
	return (route->conflicts != NULL && route->destination != NULL && route->id != NULL 
	        && route->orientation != NULL && route->path != NULL && route->points != NULL 
	        && route->sections != NULL && route->signals != NULL && route->source != NULL
			  && route->train != NULL);
}

static GArray* get_route_signal_info_array(t_interlocking_route *route) {
	// - 0. Build signal_infos array that holds information about the signals in the route. Saves...
	// 	- ID of signal
	// 	- if signal has been 'reached'/have been set to stop.
	// 	- if signal is the source signal
	// 	- if signal is the destination signal
	// 	- Index of the signal in route->path
	// 	- occurrence/count of this particular signal id in the signal info array 
	//       (useful should we ever have routes were signals can appear more than once)
	if (!validate_interlocking_route_members_not_null(route)) {
		syslog_server(LOG_WARNING, "Get route signal-info array called with route parameter NULL"
		                           ", or members of route parameter struct have value NULL");
		return NULL;
	}
	syslog_server(LOG_DEBUG, "Get route signal-info array called for routeID %s", route->id);
	const size_t path_count = route->path->len;
	const size_t number_of_signal_infos = route->signals->len;
	GArray *signal_infos = g_array_sized_new(FALSE, FALSE, 
	                                         sizeof(t_route_signal_info), 
	                                         number_of_signal_infos);
	size_t built_signal_infos_counter = 0;
	for (size_t i = 0; i < number_of_signal_infos; ++i) {
		const char *signal_id_item = g_array_index(route->signals, char *, i);
		if (signal_id_item == NULL) {
			syslog_server(LOG_WARNING, "Get route signal-info array"
			                           " found invalid (NULL) signal, skipped");
			continue; // skip loop iteration
		}
		t_route_signal_info *signal_info_elem = malloc(sizeof(t_route_signal_info));
		if (signal_info_elem == NULL) {
			syslog_server(LOG_ERR, "Get route signal-info array, "
			                       "unable to allovate memory for new signal info element");
			return NULL;
		}
		// signal_info basic properties
		signal_info_elem->has_been_set_to_stop = false;
		signal_info_elem->is_source_signal = (i == 0);
		signal_info_elem->is_destination_signal = (i+1 == number_of_signal_infos);
		
		// signal_info->id
		signal_info_elem->id = malloc(sizeof(char) * (strlen(signal_id_item) + 1));
		if (signal_info_elem->id == NULL) {
			syslog_server(LOG_ERR, "Get route signal-info array, "
			                        "unable to allovate memory for new signal info element ID");
			return NULL;
		}
		strcpy(signal_info_elem->id, signal_id_item);
		
		// signal_info->number_of_occurrence
		if (i == 0) {
			// First iteration -> signal must be occurring for the first time.
			signal_info_elem->number_of_occurrence = 0;
		} else {
			// Occurrence-counter of id in this signal_infos array, 
			// determine by searching in iteration of start -> current position
			size_t signal_internal_prior_occurrences_count = 0;
			for (size_t siginfo_index_intern = 0; siginfo_index_intern < i; ++siginfo_index_intern) {
				const t_route_signal_info *siginfo_search_item = g_array_index(signal_infos, 
				                                                               t_route_signal_info*,
				                                                               siginfo_index_intern);
				if (siginfo_search_item != NULL) {
					if (strcmp(siginfo_search_item->id, signal_info_elem->id) == 0) {
						signal_internal_prior_occurrences_count++;
					}
				}
			}
			signal_info_elem->number_of_occurrence = signal_internal_prior_occurrences_count;
		}
		// signal_info->index_in_route_path
		// index of the signal in route->path (only if not source or destination signal?) 
		// determine by iterating/searching route->path
		size_t found_counter = 0;
		for (size_t path_index = 0; path_index < path_count; ++path_index) {
			const char *path_item = g_array_index(route->path, char *, path_index);
			if (path_item != NULL) {
				if (strcmp(path_item, signal_id_item) == 0) {
					if (found_counter == signal_info_elem->number_of_occurrence) {
						signal_info_elem->index_in_route_path = path_index;
						break;
					}
					found_counter++;
				}
			}
		}
		++built_signal_infos_counter;
		g_array_append_val(signal_infos, signal_info_elem);
	}
	syslog_server(LOG_DEBUG, "Get route signal-info array returning "
	                         "with %d built signal-info elements", built_signal_infos_counter);
	return signal_infos;
}

static void free_route_signal_info_array(GArray *route_signal_infos) {
	if (route_signal_infos == NULL) {
		return;
	}
	const size_t arr_len = route_signal_infos->len;
	for (size_t signal_info_index = 0; signal_info_index < arr_len; ++signal_info_index) {
		t_route_signal_info *signal_info_search_item = g_array_index(route_signal_infos, 
		                                                             t_route_signal_info*, 
		                                                             signal_info_index);
		if (signal_info_search_item != NULL && signal_info_search_item->id != NULL) {
			free(signal_info_search_item->id);
		}
	}
	g_array_free(route_signal_infos, true);
}

static bool drive_route_progressive_stop_signals_decoupled(const char *train_id, t_interlocking_route *route) {
	// Strategy:
	// Route tells us the signals involved in it. Both on their own, and in the path.
	// During driving (while train route driving valid, or other condition):
	// - 1. Get position of train
	// - 2. Determine index (in route->path) of the segment where the train is ('train index')
	// - 3. Determine which signals have a lower index in route->path than the 'train index'
	//      Set these 'passed signals' to aspect_stop.
	//        Note: Do not check the *actual* current aspect of these signals, as with 
	//        sectional route release, already passed signals may have been set back 
	//        to GO for another route.
	if (route == NULL || route->id == NULL) {
		syslog_server(LOG_ERR, "drive route progressive stop signals decoupled, "
		                       "route or route->id is NULL");
	}
	syslog_server(LOG_DEBUG, "drive route progressive stop signals decoupled "
									 "called for routeID %s", route->id);
	// Get route signal infos
	GArray *signal_info_array = get_route_signal_info_array(route);
	// Check that signal_infos array is not empty and has at least 2 entries (source, destination)
	if (signal_info_array == NULL) {
		syslog_server(LOG_ERR, "drive route progressive stop signals decoupled "
		                        "unable to retrieve signal-info array for route to be driven");
		return false;
	} else if (signal_info_array->len < 2) {
		free_route_signal_info_array(signal_info_array);
		syslog_server(LOG_ERR, "drive route progressive stop signals decoupled "
		                       "got signal-info array, but this has not enough signal infos (< 2)");
		return false;
	}
	syslog_server(LOG_DEBUG, "drive route progressive stop signals decoupled got signal-info array "
									 "with length %d for routeID %s", signal_info_array->len, route->id);
	const char *signal_stop_aspect = "aspect_stop";
	const size_t path_count = route->path->len;
	size_t train_pos_index_previous = 0;
	// Route driving ongoing/starts
	while (running && drive_route_params_valid(train_id, route)) {
		// 1. Get position of train
		t_bidib_train_position_query train_position_query = bidib_get_train_position(train_id);
		// 2. Determine index (in route->path) of the segment where the train is ('train index')
		///TODO: Protect against 'too-far-ahead' recognition if one segment appears more than once in one route.
		//  potential strategies
		//   - use second-most-forward segment (no use if train occupies only one)
		//   - ignore segments that appear twice (use prev. train pos or none)
		//   - compare change in position to previous iteration (no use if train is set further ahead by hand)
		size_t train_pos_index = 0;
		for (size_t path_item_index = 0; path_item_index < path_count; path_item_index) {
			const char *path_item = g_array_index(route->path, char *, path_item_index);
			for (size_t i = 0; i < train_position_query.length; i++) {
				if (strcmp(path_item, train_position_query.segments[i]) == 0
				    && path_item_index > train_pos_index) {
						train_pos_index = path_item_index;
				}
			}
		}
		bidib_free_train_position_query(train_position_query);
		// If max_index_occ_segment is still 0, assume train has yet to enter the route.
		// If previous train position is the same, do nothing. Otherwise, look for signals to set to stop.
		if (train_pos_index > 0 && train_pos_index_previous != train_pos_index) {
			// 3. Determine which signals have a lower index in route->path than the 'train index'
			//    and set them to aspect_stop.
			const size_t arr_len = signal_info_array->len;
			size_t signals_set_stop_this_iter_counter = 0;
			for (size_t signal_info_index = 0; signal_info_index < arr_len; ++signal_info_index) {
				t_route_signal_info *signal_info_search_item = g_array_index(signal_info_array, 
				                                                             t_route_signal_info*, 
				                                                             signal_info_index);
				if (signal_info_search_item != NULL && !signal_info_search_item->has_been_set_to_stop 
				    && signal_info_search_item->index_in_route_path < train_pos_index) {
					
					if (bidib_set_signal(signal_info_search_item->id, signal_stop_aspect)) {
						// Log_ERR... but what else? Safety violation once we have a safety layer?
						syslog_server(LOG_ERR, "drive route progressive stop signals decoupled unable "
														"to set signal %s to stop.", signal_info_search_item->id);
					} else {
						signal_info_search_item->has_been_set_to_stop = true;
						signals_set_stop_this_iter_counter++;
					}
				}
			}
			bidib_flush();
			syslog_server(LOG_DEBUG, "drive route progressive stop signals decoupled set %d signals to "
			                         "stop in one loop/check iteration.", signals_set_stop_this_iter_counter);
		}
		train_pos_index_previous = train_pos_index;
		usleep(TRAIN_DRIVE_TIME_STEP);
	}
	syslog_server(LOG_DEBUG, "drive route progressive stop signals decoupled for routeID %s ending.",
	                          route->id);
	free_route_signal_info_array(signal_info_array);
	return true;
}

static bool drive_route_progressive_stop_signals(const char *train_id, t_interlocking_route *route) {
	const char *signal_stop_aspect = "aspect_stop";
	const char *next_signal = route->source;
	bool set_signal_stop = true;
	const int path_count = route->path->len;
	for (size_t path_item_index = 0; path_item_index < path_count; path_item_index++) {
		// Get path item (segment or signal)
		const char *path_item = g_array_index(route->path, char *, path_item_index);
		
		if (is_type_signal(path_item)) {
			// Train will encounter a signal when it exits the current segment
			next_signal = path_item;
			set_signal_stop = true;
		}
		
		if (set_signal_stop && is_type_segment(path_item)) {
			// Signal that the train has just passed will be set to the Stop aspect 
			// when it enters the next segment

			// Wait until the next segment is entered
			while (running && !train_position_is_at(train_id, path_item)) {
				usleep(TRAIN_DRIVE_TIME_STEP);
				route = get_route(route->id);
				if (!drive_route_params_valid(train_id, route)) {
					return false;
				}
			}
			
			// Set signal to the Stop aspect
			set_signal_stop = false;
			if (bidib_set_signal(next_signal, signal_stop_aspect)) {
				syslog_server(LOG_ERR, "Drive route progressive stop signals: "
				              "Unable to set route signal %s to aspect %s", 
				              next_signal, signal_stop_aspect);
			} else {
				bidib_flush();
				syslog_server(LOG_NOTICE, "Drive route progressive stop signals: "
				              "Set signal - signal: %s state: %s",
				              next_signal, signal_stop_aspect);
			}
		}
	}
	
	return true;
}

static bool drive_route(const int grab_id, const char *route_id, const bool is_automatic) {
	char *train_id = strdup(grabbed_trains[grab_id].name->str);
	t_interlocking_route *route = get_route(route_id);
	if (!drive_route_params_valid(train_id, route)) {
		return false;
	}

	// Driving starts: Driving direction is computed from the route orientation
	syslog_server(LOG_NOTICE, "Drive route: Driving starts");
	pthread_mutex_lock(&grabbed_trains_mutex);	
	const int engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
	const char requested_forwards = is_forward_driving(route, train_id);
	if (is_automatic) {
		dyn_containers_set_train_engine_instance_inputs(engine_instance,
		                                                DRIVING_SPEED_SLOW, 
		                                                requested_forwards);
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	
	// Set the signals along the route to Stop as the train drives past them
	const bool result = drive_route_progressive_stop_signals(train_id, route);
	
	// Wait for train to reach the end of the route
	const char *dest_segment = g_array_index(route->path, char *, route->path->len - 1);
	while (running && result && !train_position_is_at(train_id, dest_segment)
	                         && drive_route_params_valid(train_id, route)) {
		usleep(TRAIN_DRIVE_TIME_STEP);
		route = get_route(route->id);
	}
	
	// Driving stops
	pthread_mutex_lock(&grabbed_trains_mutex);
	syslog_server(LOG_NOTICE, "Drive route: Driving stops");
	dyn_containers_set_train_engine_instance_inputs(engine_instance, 0, requested_forwards);
	pthread_mutex_unlock(&grabbed_trains_mutex);
	
	// Release the route
	if (drive_route_params_valid(train_id, route)) {
		release_route(route_id);
	}
	
	free(train_id);
	return true;
}

static int grab_train(const char *train, const char *engine) {
	pthread_mutex_lock(&grabbed_trains_mutex);
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		if (grabbed_trains[i].is_valid && strcmp(grabbed_trains[i].name->str, train) == 0) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			syslog_server(LOG_ERR, "Train %s already grabbed", train);
			return -1;
		}
	}
	int start = next_grab_id;
	if (grabbed_trains[next_grab_id].is_valid) {
		increment_next_grab_id();
		while (grabbed_trains[next_grab_id].is_valid) {
			if (next_grab_id == start) {
				pthread_mutex_unlock(&grabbed_trains_mutex);
				syslog_server(LOG_ERR, "All grab ids in use");
				return -1;
			}
			increment_next_grab_id();
		}
	}
	int grab_id = next_grab_id;
	increment_next_grab_id();
	grabbed_trains[grab_id].name = g_string_new(train);
	strcpy(grabbed_trains[grab_id].track_output, "master");
	if (dyn_containers_set_train_engine_instance(&grabbed_trains[grab_id], train, engine)) {
		pthread_mutex_unlock(&grabbed_trains_mutex);
		syslog_server(LOG_ERR, "Train engine %s could not be used", engine);
		return -1;
	}
	grabbed_trains[grab_id].is_valid = true;
	pthread_mutex_unlock(&grabbed_trains_mutex);
	syslog_server(LOG_NOTICE, "Train %s grabbed", train);
	return grab_id;
}

bool release_train(int grab_id) {
	bool success = false;
	pthread_mutex_lock(&grabbed_trains_mutex);
	if (grabbed_trains[grab_id].is_valid) {
		grabbed_trains[grab_id].is_valid = false;
		dyn_containers_free_train_engine_instance(grabbed_trains[grab_id].dyn_containers_engine_instance);
		syslog_server(LOG_NOTICE, "Train %s released", grabbed_trains[grab_id].name->str);
		g_string_free(grabbed_trains[grab_id].name, TRUE);
		grabbed_trains[grab_id].name = NULL;
		success = true;
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	return success;
}

void release_all_grabbed_trains(void) {
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		release_train(i);
	}
}

onion_connection_status handler_grab_train(void *_, onion_request *req,
                                           onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const char *data_engine = onion_request_get_post(req, "engine");		
		if (data_train == NULL || data_engine == NULL) {
			syslog_server(LOG_ERR, "Request: Grab train - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			t_bidib_train_state_query train_state_query =
				bidib_get_train_state(data_train);
			if (!train_state_query.known) {
				bidib_free_train_state_query(train_state_query);
				syslog_server(LOG_ERR, "Request: Grab train - train not known");
				return OCS_NOT_IMPLEMENTED;
			} else {
				bidib_free_train_state_query(train_state_query);
				int grab_id = grab_train(data_train, data_engine);
				if (grab_id == -1) {
					//TODO more precise error message if all slots are taken
					syslog_server(LOG_ERR, "Request: Grab train - train already grabbed or engine not found");
					return OCS_NOT_IMPLEMENTED;
				} else {
					syslog_server(LOG_NOTICE, "Request: Grab train - train: %s", data_train);
					onion_response_printf(res, "%ld,%d", session_id, grab_id);
					return OCS_PROCESSED;
				}
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Grab train - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_release_train(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {		
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		int client_session_id = params_check_session_id(data_session_id);
		int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Release train - invalid session id (%s != %d)",
			              data_session_id, session_id);
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Release train - invalid grab id");
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
			syslog_server(LOG_ERR, "Request: Release train - invalid grab id");
			return OCS_NOT_IMPLEMENTED;
		} else {
			syslog_server(LOG_NOTICE, "Request: Release train");
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Release train - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_request_route(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_source_name = onion_request_get_post(req, "source");
		const char *data_destination_name = onion_request_get_post(req, "destination");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Request train route - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Request train route - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_source_name == NULL || data_destination_name == NULL) {
			syslog_server(LOG_ERR, "Request: Request train route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			// Use interlocker to find and grant a route
			
			GString *route_id = grant_route(grabbed_trains[grab_id].name->str,
			                                data_source_name,
			                                data_destination_name);
			if (route_id->str != NULL && params_check_is_number(route_id->str)) {
				syslog_server(LOG_NOTICE, "Request: Request train route - "
				              "train: %s route: %s",
				              grabbed_trains[grab_id].name->str, route_id->str);
				onion_response_printf(res, "%s", route_id->str);
				g_string_free(route_id, true);
				return OCS_PROCESSED;
			} else {
				syslog_server(LOG_ERR, "Request: Request train route - "
				              "train: %s route not granted",
				              grabbed_trains[grab_id].name->str);
				if (strcmp(route_id->str, "no_interlocker") == 0) {
					onion_response_printf(res, "No interlocker has been selected for use");
				} else if (strcmp(route_id->str, "no_routes") == 0) {
					onion_response_printf(res, "No routes possible from %s to %s", 
					                      data_source_name, data_destination_name);
				} else if (strcmp(route_id->str, "not_grantable") == 0) {
					onion_response_printf(res, "Route found conflicts with others");
				} else if (strcmp(route_id->str, "not_clear") == 0) {
					onion_response_printf(res, "Route found has occupied tracks "
					                      "or source signal is not stop");
				} else {
					onion_response_printf(res, "Route could not be granted (%s)",
					                      route_id->str);
				}
				g_string_free(route_id, true);

				onion_response_set_code(res, HTTP_BAD_REQUEST);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Request train route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}                                       
}

onion_connection_status handler_request_route_id(void *_, onion_request *req,
                                                 onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const char *route_id = params_check_route_id(data_route_id);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Request train route - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Request train route - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (strcmp(route_id, "") == 0) {
			syslog_server(LOG_ERR, "Request: Request train route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			// Grant the route ID using an internal algorithm
			
			const char *result = grant_route_id(grabbed_trains[grab_id].name->str,
			                                    route_id);
			if (strcmp(result, "granted") == 0) {
				syslog_server(LOG_NOTICE, "Request: Request train route - "
				              "train: %s route: %s",
				              grabbed_trains[grab_id].name->str, route_id);
				onion_response_printf(res, "%s", result);
				return OCS_PROCESSED;
			} else {
				syslog_server(LOG_ERR, "Request: Request train route - "
				              "train: %s route: %s not granted",
				              grabbed_trains[grab_id].name->str, route_id);
				if (strcmp(result, "not_grantable") == 0) {
					onion_response_printf(res, "Route found is not available "
					                      "or has conflicts with others");
				} else if (strcmp(result, "not_clear") == 0) {
					onion_response_printf(res, "Route found has occupied tracks "
					                      "or source signal is not stop");
				} else {
					onion_response_printf(res, "Route could not be granted (%s)",
					                      route_id);
				}

				onion_response_set_code(res, HTTP_BAD_REQUEST);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Request train route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}                                       
}

onion_connection_status handler_driving_direction(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Driving direction - bad train id");
			return OCS_NOT_IMPLEMENTED;
		} else if (strcmp(route_id, "") == 0) {
			syslog_server(LOG_ERR, "Request: Driving direction - bad route id");
			return OCS_NOT_IMPLEMENTED;
		} else {
			const t_interlocking_route *route = get_route(route_id);
			const char *direction = is_forward_driving(route, data_train)
			                        ? "forwards" : "backwards";
			onion_response_printf(res, "%s", direction);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Driving direction - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}  
}

onion_connection_status handler_drive_route(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *data_mode = onion_request_get_post(req, "mode");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const char *route_id = params_check_route_id(data_route_id);
		const char *mode = params_check_mode(data_mode);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Drive route - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Drive route - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (strcmp(mode, "") == 0) {
			syslog_server(LOG_ERR, "Request: Drive route - bad driving mode");
			return OCS_NOT_IMPLEMENTED;			
		} else if (strcmp(route_id, "") == 0) {
			syslog_server(LOG_ERR, "Request: Drive route - bad route id");
			return OCS_NOT_IMPLEMENTED;
		} else {
			const bool is_automatic = (strcmp(mode, "automatic") == 0);
			if (drive_route(grab_id, route_id, is_automatic)) {
				onion_response_printf(res, "Route %s driving completed", route_id);
				return OCS_PROCESSED;
			} else {
				return OCS_NOT_IMPLEMENTED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Drive route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}  
}

onion_connection_status handler_set_dcc_train_speed(void *_, onion_request *req,
                                                    onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_speed = onion_request_get_post(req, "speed");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		int client_session_id = params_check_session_id(data_session_id);
		int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		int speed = params_check_speed(data_speed);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Set train speed - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Set train speed - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (speed == 999) {
			syslog_server(LOG_ERR, "Request: Set train speed - bad speed");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, "Request: Set train speed - bad track output");
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
		syslog_server(LOG_ERR, "Request: Set train speed - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_calibrated_train_speed(void *_,
                                                           onion_request *req,
                                                           onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_speed = onion_request_get_post(req, "speed");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		int client_session_id = params_check_session_id(data_session_id);
		int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		int speed  = params_check_calibrated_speed(data_speed);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Set calibrated train speed - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Set calibrated train speed - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (speed == 999) {
			syslog_server(LOG_ERR, "Request: Set calibrated train speed - bad speed");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, "Request: Set calibrated train speed - bad track output");
			return OCS_NOT_IMPLEMENTED;
		} else {
			pthread_mutex_lock(&grabbed_trains_mutex);
			if (bidib_set_calibrated_train_speed(grabbed_trains[grab_id].name->str,
		                                         speed, data_track_output)) {
				syslog_server(LOG_ERR, "Request: Set calibrated train speed - bad "
				              "parameter values");
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_NOT_IMPLEMENTED;
			} else {
				bidib_flush();
				syslog_server(LOG_NOTICE, "Request: Set calibrated train speed - "
				              "train: %s speed: %d",
				              grabbed_trains[grab_id].name->str, speed);
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set calibrated train speed - system not running "
		       "or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_train_emergency_stop(void *_,
                                                         onion_request *req,
                                                         onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		int client_session_id = params_check_session_id(data_session_id);
		int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Set train emergency stop - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Set train emergency stop - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, "Request: Set train emergency stop - bad track output");
			return OCS_NOT_IMPLEMENTED;
		} else {
			pthread_mutex_lock(&grabbed_trains_mutex);
			if (bidib_emergency_stop_train(grabbed_trains[grab_id].name->str,
		                                   data_track_output)) {
				syslog_server(LOG_ERR, "Request: Set train emergency stop - bad "
				              "parameter values");
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_NOT_IMPLEMENTED;
			} else {
				bidib_flush();
				syslog_server(LOG_NOTICE, "Request: Set train emergency stop - train: %s",
				              grabbed_trains[grab_id].name->str);
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set train emergency stop - system not running "
		              "or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_train_peripheral(void *_,
                                                     onion_request *req,
                                                     onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_peripheral = onion_request_get_post(req, "peripheral");
		const char *data_state = onion_request_get_post(req, "state");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		int client_session_id = params_check_session_id(data_session_id);
		int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		int state = params_check_state(data_state);
		if (client_session_id != session_id) {
			syslog_server(LOG_ERR, "Request: Set train peripheral - invalid session id");
			return OCS_NOT_IMPLEMENTED;
		} else if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			syslog_server(LOG_ERR, "Request: Set train peripheral - bad grab id");
			return OCS_NOT_IMPLEMENTED;
		} else if (state == -1) {
			syslog_server(LOG_ERR, "Request: Set train peripheral - bad state");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_peripheral == NULL) {
			syslog_server(LOG_ERR, "Request: Set train peripheral - bad peripheral");
			return OCS_NOT_IMPLEMENTED;
		} else if (data_track_output == NULL) {
			syslog_server(LOG_ERR, "Request: Set train peripheral - bad track output");
			return OCS_NOT_IMPLEMENTED;
		} else {
			pthread_mutex_lock(&grabbed_trains_mutex);
			if (bidib_set_train_peripheral(grabbed_trains[grab_id].name->str,
			                               data_peripheral, state,
			                               data_track_output)) {
				syslog_server(LOG_ERR, "Request: Set train peripheral - bad "
				              "parameter values");
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_NOT_IMPLEMENTED;
			} else {
				bidib_flush();
				syslog_server(LOG_NOTICE, "Request: Set train peripheral - train: %s "
				              "peripheral: %s state: 0x%02x",
				              grabbed_trains[grab_id].name->str,
				              data_peripheral, state);
				pthread_mutex_unlock(&grabbed_trains_mutex);
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Set train peripheral - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

