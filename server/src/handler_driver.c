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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 *
 */

#include <bidib/bidib.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "handler_driver.h"
#include "server.h"
#include "handler_controller.h"
#include "dyn_containers_interface.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"
#include "json_response_builder.h"
#include "communication_utils.h"

pthread_mutex_t grabbed_trains_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int DRIVING_SPEED_SLOW = 40;
static unsigned int DRIVING_SPEED_STOPPING = 25;
static unsigned int next_grab_id = 0;


typedef struct {
	char *id;
	bool has_been_set_to_stop;
	bool is_source_signal;
	bool is_destination_signal;
	unsigned int index_in_route_path;
} t_route_signal_info;

typedef struct {
	t_route_signal_info **data_ptr;
	unsigned int len;
} t_route_signal_info_array;

t_train_data grabbed_trains[TRAIN_ENGINE_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_engine_instance = -1 }
};

typedef struct {
	bool *arr;
	unsigned int len;
} t_route_repeated_segment_flags;

typedef enum {
	ERR_INVALID_PARAM,
	ERR_TRAIN_NOT_ON_TRACKS,
	ERR_TRAIN_NOT_ON_ROUTE,
	OKAY_TRAIN_ON_ROUTE
} e_route_pos_error_code;

typedef struct {
	unsigned int pos_index;
	e_route_pos_error_code err_code;
} t_train_index_on_route_query;

static void increment_next_grab_id(void) {
	if (next_grab_id == TRAIN_ENGINE_INSTANCE_COUNT_MAX - 1) {
		next_grab_id = 0;
	} else {
		next_grab_id++;
	}
}

int train_get_grab_id(const char *train) {
	if (train == NULL) {
		syslog_server(LOG_ERR, "Train get grab-id - invalid (NULL) train");
		return -1;
	}
	int grab_id = -1;
	pthread_mutex_lock(&grabbed_trains_mutex);
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		if (grabbed_trains[i].is_valid && strcmp(grabbed_trains[i].name->str, train) == 0) {
			grab_id = i;
			break;
		}
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	return grab_id;
}

bool train_grabbed(const char *train) {
	if (train == NULL) {
		syslog_server(LOG_ERR, "Train grabbed - invalid (NULL) train");
		return false;
	}
	bool grabbed = false;
	pthread_mutex_lock(&grabbed_trains_mutex);
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
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

static bool train_position_is_at(const char *train_id, const char *segment) {
	if (train_id == NULL || segment == NULL) {
		syslog_server(LOG_ERR, "Train position is at - invalid (NULL) parameters");
		return false;
	}
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

static bool is_forward_driving(const t_interlocking_route *route, const char *train_id) {
	if (route == NULL || train_id == NULL) {
		syslog_server(LOG_ERR, "Is forward driving - invalid (NULL) parameters");
		return true;
	}
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
		if (block_id != NULL && strlen(block_id) > 0) {
			break;
		}
	}
	bidib_free_train_position_query(train_position_query);
	
	if (block_id == NULL || strlen(block_id) == 0) {
		syslog_server(LOG_ERR, 
		              "Is forward driving - train: %s driving: %s - current block of train unknown",
		              train_id, is_forwards ? "forwards" : "backwards");
		return is_forwards;
	}
	
	// 2. Check whether the train is on a block controlled by a reverser
	bool block_reversed = false;
	t_bidib_id_list_query rev_query = bidib_get_connected_reversers();
	for (size_t i = 0; i < rev_query.length; i++) {
		const char *reverser_id = rev_query.ids[i];
		const char *reverser_block = 
				config_get_scalar_string_value("reverser", reverser_id, "block");
		
		if (strcmp(block_id, reverser_block) == 0) {
			const bool succ = reversers_state_update();
			t_bidib_reverser_state_query rev_state_query = bidib_get_reverser_state(reverser_id);
			// 3. Check the reverser's state
			if (succ && rev_state_query.available) {
				block_reversed = (rev_state_query.data.state_value == BIDIB_REV_EXEC_STATE_ON);
			}
			bidib_free_reverser_state_query(rev_state_query);
			break;
		}
	}
	bidib_free_id_list_query(rev_query);
	
	const bool requested_forwards = block_reversed ? !is_forwards : is_forwards;
	syslog_server(LOG_NOTICE, 
	              "Is forward driving - train: %s driving: %s",
	              train_id, requested_forwards ? "forwards" : "backwards");
	return requested_forwards;
}

static bool drive_route_params_valid(const char *train_id, t_interlocking_route *route) {
	if (train_id == NULL || route == NULL) {
		syslog_server(LOG_ERR, "Check drive route params - invalid (NULL) parameters");
		return false;
	} else if (route->train == NULL || strcmp(train_id, route->train) != 0) {
		syslog_server(LOG_WARNING, 
		              "Check drive route params - route %s not granted to train %s", 
		              route->id, train_id);
		return false;
	}
	return true;
}

static bool validate_interlocking_route_members_not_null(const t_interlocking_route *route) {
	if (route == NULL) {
		return false;
	}
	return (route->conflicts != NULL && route->destination != NULL && route->id != NULL 
	        && route->orientation != NULL && route->path != NULL && route->points != NULL 
	        && route->sections != NULL && route->signals != NULL && route->source != NULL
	        && route->train != NULL);
}

static void free_route_signal_info_array(t_route_signal_info_array *route_signal_info_array) {
	if (route_signal_info_array == NULL) {
		return;
	}
	if (route_signal_info_array->data_ptr == NULL) {
		route_signal_info_array->len = 0;
		return;
	}
	for (unsigned int i = 0; i < route_signal_info_array->len; ++i) {
		t_route_signal_info *elem = route_signal_info_array->data_ptr[i];
		if (elem != NULL) {
			if (elem->id != NULL) {
				free(elem->id);
			}
			free(elem);
		}
	}
	free(route_signal_info_array->data_ptr);
	route_signal_info_array->data_ptr = NULL;
	route_signal_info_array->len = 0;
	// Don't free route_signal_info_array itself, as we have not allocated it with malloc.
}

// For the signal with id signal_id_item, populates the member of signal_info_array->data_ptr at
// position index_in_info_array if possible and updates the length of signal_info_array.
// Returns false if critical error was encountered and signal_info_array is useless for 
// continuing. Otherwise returns true.
static bool add_signal_info_for_signal(t_route_signal_info_array *signal_info_array, 
                                       const char *signal_id_item, 
                                       unsigned int index_in_info_array, 
                                       unsigned int number_of_signal_infos) {
	if (signal_info_array == NULL || signal_info_array->data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Add signal-info to signal_info_array - "
		              "signal info array or signal info array data pointer is NULL");
		return false;
	}
	const unsigned int i = index_in_info_array;
	// A. Return without adding signal-info if signal from route->signals is NULL
	if (signal_id_item == NULL) {
		syslog_server(LOG_WARNING,
		              "Add signal-info to signal_info_array - "
		              "skipping NULL signal at index %u of route->signals", 
		              i);
		signal_info_array->data_ptr[i] = NULL;
		signal_info_array->len = i + 1;
		// Return true as this is not a critical error, i.e. we can still continue with route
		return true;
	}
	// B. Allocate memory for element i of signal_info_array->data_ptr
	signal_info_array->data_ptr[i] = (t_route_signal_info *) malloc(sizeof(t_route_signal_info));
	signal_info_array->len = i + 1;
	if (signal_info_array->data_ptr[i] == NULL) {
		syslog_server(LOG_ERR, 
		              "Add signal-info to signal_info_array - "
		              "unable to allocate memory for array index %u", 
		              i);
		return false;
	}
	
	// C. Initialise members in signal_info_array->data_ptr[i] to appropriate defaults
	signal_info_array->data_ptr[i]->has_been_set_to_stop = false;
	signal_info_array->data_ptr[i]->is_source_signal = (i == 0);
	signal_info_array->data_ptr[i]->is_destination_signal = (i+1 == number_of_signal_infos);
	
	// D. Copy signal id from route->signals to new route_signal_info
	signal_info_array->data_ptr[i]->id = strdup(signal_id_item);
	if (signal_info_array->data_ptr[i]->id == NULL) {
		syslog_server(LOG_ERR,
		              "Add signal-info to signal_info_array - "
		              "unable to allocate memory for signal id %s",
		              signal_id_item);
		return false;
	}
	return true;
}

static t_route_signal_info_array get_route_signal_info_array(const t_interlocking_route *route) {
	t_route_signal_info_array info_arr = {.data_ptr = NULL, .len = 0};
	if (!validate_interlocking_route_members_not_null(route)) {
		syslog_server(LOG_ERR, 
		             "Get route signal info array - route is NULL or some route details are NULL");
		return info_arr;
	}
	
	// 1. Allocate memory for array of pointers to t_route_signal_info type entities
	const unsigned int number_of_signal_infos = route->signals->len;
	info_arr.data_ptr = 
			(t_route_signal_info**) malloc(sizeof(t_route_signal_info*) * number_of_signal_infos);
	
	if (info_arr.data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Get route signal info array - route: %s - could not allocate memory for array",
		              route->id);
		return info_arr;
	}
	// 2. For every signal in route->signals...
	for (unsigned int i = 0; i < number_of_signal_infos; ++i) {
		const char *signal_id_item = g_array_index(route->signals, char *, i);
		// ... Call function to add a signal-info to info_arr for signal_id_item
		if (!add_signal_info_for_signal(&info_arr, signal_id_item, i, number_of_signal_infos)) {
			syslog_server(LOG_ERR, 
			              "Get route signal info array - route: %s - "
			              "failed to add signal-info, abort",
			              route->id);
			free_route_signal_info_array(&info_arr);
			return (t_route_signal_info_array){.data_ptr = NULL, .len = 0};
		}
	}
	// 3. For every signal_info, determine index of its signal-id in route->path and set member 
	//    for index_in_route_path of signal_info accordingly
	unsigned int path_index = 0;
	for (unsigned int i = 0; i < info_arr.len; ++i) {
		if (info_arr.data_ptr[i] == NULL) {
			syslog_server(LOG_WARNING, 
			              "Get route signal info array - route: %s - "
			              "skipped NULL signal_info at pos %u of info_arr", 
			              route->id, i);
			continue;
		}
		
		info_arr.data_ptr[i]->index_in_route_path = 0;
		// source and destination signals do not appear in route->path, thus skip them here
		if (info_arr.data_ptr[i]->is_source_signal || info_arr.data_ptr[i]->is_destination_signal) {
			continue;
		}
		// path_index not reset on purpose, as info_arr signals are ordered ascendingly
		// in relation to their occurrence in the route.
		for (; path_index < route->path->len; ++path_index) {
			const char *path_item = g_array_index(route->path, char *, path_index);
			if (path_item != NULL && strcmp(path_item, info_arr.data_ptr[i]->id) == 0) {
				info_arr.data_ptr[i]->index_in_route_path = path_index;
				break;
			}
		}
	}
	return info_arr;
}

static void free_route_repeated_segment_flags(t_route_repeated_segment_flags *r_seg_flags) {
	if (r_seg_flags != NULL && r_seg_flags->arr != NULL) {
		free(r_seg_flags->arr);
		r_seg_flags->arr = NULL;
		r_seg_flags->len = 0;
	}
}

static t_route_repeated_segment_flags get_route_repeated_segment_flags(const t_interlocking_route *route) {
	// For route, determine which segments appear more than once.
	
	t_route_repeated_segment_flags r_seg_flags = {.arr = NULL, .len = 0};
	if (route == NULL || route->path == NULL) {
		return r_seg_flags;
	}
	const unsigned int route_path_len = route->path->len;
	r_seg_flags.arr = malloc(sizeof(bool) * route_path_len);
	if (r_seg_flags.arr == NULL) {
		syslog_server(LOG_ERR, 
		              "Get repeated segment flags - route: %s - unable to allocate memory for array",
		              route->id);
		return r_seg_flags;
	}
	r_seg_flags.len = route_path_len;
	
	for (unsigned int i = 0; i < r_seg_flags.len; ++i) {
		r_seg_flags.arr[i] = false;
	}
	
	// For every segment in route->path, check if it occurs again at a different position. 
	// If yes, set the respective flags in r_seg_flags to true.
	
	for (unsigned int path_index_i = 0; path_index_i < route_path_len; ++path_index_i) {
		const char *path_item_i = g_array_index(route->path, char *, path_index_i);
		if (path_item_i != NULL && is_type_segment(path_item_i) && (path_index_i + 1) < route_path_len) {
			for (unsigned int path_index_n = path_index_i + 1; path_index_n < route_path_len; ++path_index_n) {
				const char *path_item_n = g_array_index(route->path, char *, path_index_n);
				if (path_item_n != NULL && strcmp(path_item_i, path_item_n) == 0) {
					r_seg_flags.arr[path_index_i] = true;
					r_seg_flags.arr[path_index_n] = true;
					break;
				}
			}
		}
	}
	return r_seg_flags;
}

// Queries the position of the train. Checks which segments that the train occupies exist in the route path. 
// If train occupies at least one segment in route path that does not occur more than once in route,
// returned t_train_index_on_route_query member .pos_index holds the index of the occupied segment 
// that is the furthest along the route, and .err_code holds OKAY_TRAIN_ON_ROUTE
// If parameters are invalid, t_train_index_on_route_query .err_code holds ERR_INVALID_PARAM.
// If train is not on tracks, t_train_index_on_route_query .err_code holds ERR_TRAIN_NOT_ON_TRACKS.
// If train is not on route, t_train_index_on_route_query .err_code holds ERR_TRAIN_NOT_ON_ROUTE.
static t_train_index_on_route_query get_train_pos_index_in_route_ignore_repeated_segments(const char *train_id, 
                                                              const t_interlocking_route *route, 
                                                              const t_route_repeated_segment_flags *repeated_segment_flags) {
	t_train_index_on_route_query ret_query = {.pos_index = 0, .err_code = ERR_INVALID_PARAM};
	if (train_id == NULL || route == NULL || route->path == NULL || repeated_segment_flags == NULL
	        || repeated_segment_flags->arr == NULL) {
		return ret_query;
	}
	
	t_bidib_train_position_query train_pos_query = bidib_get_train_position(train_id);
	if (train_pos_query.segments == NULL || train_pos_query.length == 0) {
		ret_query.err_code = ERR_TRAIN_NOT_ON_TRACKS;
		return ret_query;
	}
	
	ret_query.err_code = ERR_TRAIN_NOT_ON_ROUTE;
	const unsigned int path_count = route->path->len;
	for (unsigned int i = 0; i < train_pos_query.length; ++i) {
		// Search starting at most recent pos_index to skip unnecessary comparisons
		if ((ret_query.pos_index + 1) >= path_count) {
			// pos_index at max, stop.
			break;
		}
		for (unsigned int n = ret_query.pos_index + 1; n < path_count; ++n) {
			bool ignore = repeated_segment_flags->arr[n];
			if (!ignore) {
				const char *path_item = g_array_index(route->path, char *, n);
				if (strcmp(path_item, train_pos_query.segments[i]) == 0) {
					ret_query.pos_index = n;
					ret_query.err_code = OKAY_TRAIN_ON_ROUTE;
					break;
				}
			}
		}
	}
	bidib_free_train_position_query(train_pos_query);
	return ret_query;
}

// For a train at position train_pos_index in route->path, set all signals to stop that
// the train has passed and have not yet been set to stop.
// Returns the count of how many signals have been set to stop in this function
static unsigned int update_route_signals_for_train_pos(t_route_signal_info_array *signal_info_array, 
                                                       t_interlocking_route *route,
                                                       unsigned int train_pos_index) {
	unsigned int signals_set_to_stop = 0;
	const char *signal_stop_aspect = "aspect_stop";
	// 1. For every signal on the route, represented by an entry in signal_info_array
	for (unsigned int sig_info_index = 0; sig_info_index < signal_info_array->len; ++sig_info_index) {
		t_route_signal_info *sig_info = signal_info_array->data_ptr[sig_info_index];
		
		if (sig_info == NULL) {
			syslog_server(LOG_WARNING, 
			              "Update route signals - route: %s - signal_info_array[%u] is NULL", 
			              route->id, sig_info_index);
			continue;
		}
		// 2. Determine if this signal is eligible to be set to stop
		if (!(sig_info->has_been_set_to_stop) && !(sig_info->is_destination_signal)) {
			// 3. Determine if signal has been passed by the train
			if (sig_info->index_in_route_path <= train_pos_index) {
				// 4. Try to set the signal to signal_stop_aspect.
				if (bidib_set_signal(sig_info->id, signal_stop_aspect)) {
					syslog_server(LOG_WARNING, 
					              "Update route signals - route: %s signal: %s - "
					              "unable to set signal to %s",
					              route->id, sig_info->id, signal_stop_aspect);
				} else {
					bidib_flush();
					syslog_server(LOG_NOTICE, 
					              "Update route signals - route: %s signal: %s - signal set to %s",
					              route->id, sig_info->id, signal_stop_aspect);
					sig_info->has_been_set_to_stop = true;
					signals_set_to_stop++;
				}
			}
		}
	}
	return signals_set_to_stop;
}

static bool validate_route_signal_info_array(const char *route_id, 
                                             t_route_signal_info_array *signal_info_array) {
	if (signal_info_array == NULL || route_id == NULL) {
		syslog_server(LOG_ERR, "Route signal info array validation - invalid (NULL) parameters");
		return false;
	}
	// Check that signal_info_array array is not empty and has >= 2 entries (source, destination)
	if (signal_info_array->data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Route signal info array validation - route: %s - "
		              "signal info array is NULL", 
		              route_id);
		return false;
	} else if (signal_info_array->len < 2) {
		syslog_server(LOG_ERR, 
		              "Route signal info array validation - route: %s - "
		              "signal info array has only %u elements (at least two elements needed)",
		              route_id, signal_info_array->len);
		return false;
	}
	return true;
}

/**
 * @brief For a train driving a route, set the signals to stop that the train passes.
 * 
 * @param train_id The train driving the route
 * @param route The route to be driven
 * @return true if signal updating successful (all passed signals were set to stop, 
 * and all signals have been passed or the route has been released or the system is stopping), 
 * otherwise returns false. 
 */
static bool monitor_train_on_route(const char *train_id, t_interlocking_route *route) {
	if (route == NULL || route->id == NULL) {
		syslog_server(LOG_ERR, "Monitor train on route - invalid (NULL) route or route->id");
		return false;
	}
	if (train_id == NULL) {
		syslog_server(LOG_ERR, 
		              "Monitor train on route - route: %s - invalid (NULL) train_id", 
		              route->id);
		return false;
	}
	
	// 1. Get route signal infos and flags of segments that appear more than once
	t_route_signal_info_array signal_info_array = get_route_signal_info_array(route);
	t_route_repeated_segment_flags repeated_segment_flags = get_route_repeated_segment_flags(route);
	
	if (!validate_route_signal_info_array(route->id, &signal_info_array)) {
		free_route_signal_info_array(&signal_info_array);
		free_route_repeated_segment_flags(&repeated_segment_flags);
		return false;
	}
	
	// Signals in a route shall be set to stop once the train has driven passed them.
	// Destination signal is already in STOP aspect, thus signal_info_array.len - 1 signals.
	// No underflow/wraparound to be concerned about because the call to
	// `drive_route_decoupled_signal_info_array_valid` above checks that signal_info_array.len >= 2.
	const unsigned int signals_to_set_to_stop_count = signal_info_array.len - 1;
	unsigned int signals_set_to_stop = 0;
	unsigned int train_pos_index_previous = 0;
	bool first_okay_position = true;
	
	while (running && drive_route_params_valid(train_id, route) 
	               && signals_set_to_stop < signals_to_set_to_stop_count) {
		// 1. Get position of train, and determine index (in route->path) of where the train is
		t_train_index_on_route_query train_pos_query = 
				get_train_pos_index_in_route_ignore_repeated_segments(train_id, route, &repeated_segment_flags);
		
		if (train_pos_query.err_code != OKAY_TRAIN_ON_ROUTE) {
			// Train position unknown, perhaps temporarily lost -> skip this iteration. 
			usleep(TRAIN_DRIVE_TIME_STEP);
			continue;
		}
		
		// 2. If train position has changed, or the train position is OKAY for the first time,
		//    check if any signals need to be set to aspect stop 
		if (train_pos_index_previous != train_pos_query.pos_index || first_okay_position) {
			const char *path_item = g_array_index(route->path, char *, train_pos_query.pos_index);
			syslog_server(LOG_DEBUG, 
			              "Monitor train on route - route: %s train: %s - train is at index %u (%s)",
			              route->id, train_id, train_pos_query.pos_index, 
			              path_item != NULL ? path_item : "PATH-ITEM-IS-NULL");
			signals_set_to_stop += 
					update_route_signals_for_train_pos(&signal_info_array, route, 
			                                           train_pos_query.pos_index);
			first_okay_position = false;
		}
		train_pos_index_previous = train_pos_query.pos_index;
		usleep(TRAIN_DRIVE_TIME_STEP);
	}
	
	if (signals_set_to_stop >= signals_to_set_to_stop_count) {
		syslog_server(LOG_INFO,
		              "Monitor train on route - route: %s train: %s - Finished, set %u signals to stop", 
		              route->id, train_id, signals_set_to_stop);
	} else {
		syslog_server(LOG_INFO,
		              "Monitor train on route - route: %s train: %s - Exiting due to route "
		              "release or shutdown, set %u signals to stop until exit", 
		              route->id, train_id, signals_set_to_stop);
	}
	free_route_signal_info_array(&signal_info_array);
	free_route_repeated_segment_flags(&repeated_segment_flags);
	return true;
}

static bool drive_route(const char* train_id, const char *route_id, bool is_automatic) {
	if (train_id == NULL || route_id == NULL) {
		syslog_server(LOG_ERR, "Drive route - invalid (NULL) parameters");
		return false;
	}
	
	t_interlocking_route *route = get_route(route_id);
	if (route == NULL || !drive_route_params_valid(train_id, route)) {
		syslog_server(LOG_ERR, 
		              "Drive route - route: %s train: %s - "
		              "unable to start driving because route is invalid",
		              route_id, train_id);
		return false;
	}
	
	// Driving starts: Driving direction is computed from the route orientation
	syslog_server(LOG_NOTICE, 
	              "Drive route - route: %s train: %s - %s driving starts", 
	              route_id, train_id, is_automatic ? "automatic" : "manual");
	
	const bool requested_forwards = is_forward_driving(route, train_id);
	if (is_automatic) {
		set_dcc_speed_for_train_maybe_grabbed(train_id, DRIVING_SPEED_SLOW, requested_forwards, NULL);
	}
	
	// Set the signals along the route to Stop as the train drives past them
	// This will return as soon as the train has passed all but the destination signal
	const bool result = monitor_train_on_route(train_id, route);
	
	if (!result) {
		syslog_server(LOG_WARNING, 
		              "Drive route - route: %s train: %s - "
		              "monitor train on route returned false, will stop route driving early",
		              route_id, train_id);
	}
	
	// If driving is automatic, slow train down once train is near the end of the route
	if (is_automatic && result) {
		// Assumes that all routes have a path with a length of at least 2.
		const char *pre_dest_segment = g_array_index(route->path, char *, route->path->len - 2);
		bool pre_dest_seg_reached = train_position_is_at(train_id, pre_dest_segment);
		while (running && !pre_dest_seg_reached
		               && drive_route_params_valid(train_id, route)) {
			usleep(TRAIN_DRIVE_TIME_STEP);
			pre_dest_seg_reached = train_position_is_at(train_id, pre_dest_segment);
		}
		
		if (pre_dest_seg_reached) {
			syslog_server(LOG_NOTICE, 
			              "Drive route - route: %s train: %s - slowing down for end of route", 
			              route_id, train_id);
			set_dcc_speed_for_train_maybe_grabbed(train_id, DRIVING_SPEED_STOPPING, requested_forwards, NULL);
		}
	}
	
	// Wait for train to drive past the destination signal -> if this happens, stop the train.
	// Train driving past the destination signal is determined by the segment behind the 
	// destination signal becoming occupied.
	const char *dest_overlap_segment = g_array_index(route->path, char *, route->path->len - 1);
	bool dest_signal_reached = is_segment_occupied(dest_overlap_segment);
	while (result && running && !dest_signal_reached && drive_route_params_valid(train_id, route)) {
		usleep(TRAIN_DRIVE_TIME_STEP);
		dest_signal_reached = is_segment_occupied(dest_overlap_segment);
	}

	struct timespec tva, tvb;
	clock_gettime(CLOCK_MONOTONIC, &tva);
	if (dest_signal_reached) {
		syslog_server(LOG_INFO, 
		              "Drive route - route: %s train: %s - end of route (%s) reached detected at %ld.%06ld", 
		              route->id, train_id, dest_overlap_segment, tva.tv_sec, tva.tv_nsec/1000);
	} else {
		syslog_server(LOG_NOTICE, 
		              "Drive route - route: %s train: %s - route driving stops before end of "
		              "route was reached (monitoring train failed, system "
		              "shutdown or route released), at %ld.%06ld", 
		              route->id, train_id, tva.tv_sec, tva.tv_nsec/1000);
	}
	
	set_dcc_speed_for_train_maybe_grabbed(train_id, 0, requested_forwards, NULL);
	clock_gettime(CLOCK_MONOTONIC, &tvb);
	syslog_server(LOG_NOTICE, 
	              "Drive route - route: %s train: %s - driving stops (commanded at %ld.%06ld)", 
	              route_id, train_id, tvb.tv_sec, tvb.tv_nsec/1000);
	
	// Release the route if still granted to the train
	if (drive_route_params_valid(train_id, route)) {
		syslog_server(LOG_NOTICE, 
		              "Drive route - route: %s train: %s - releasing the route", 
		              route_id, train_id);
		release_route(route_id);
	}
	
	return true;
}

static int grab_train(const char *train, const char *engine) {
	if (train == NULL || engine == NULL) {
		syslog_server(LOG_ERR, "Grab train - invalid (NULL) parameters");
		return -4;
	}
	pthread_mutex_lock(&grabbed_trains_mutex);
	// Check if train is already grabbed
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		if (grabbed_trains[i].is_valid && strcmp(grabbed_trains[i].name->str, train) == 0) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			syslog_server(LOG_ERR, 
			              "Grab train - train: %s engine: %s - train already grabbed", 
			              train, engine);
			return -3;
		}
	}
	// Check if there is an unused grab-id
	const int start = next_grab_id;
	if (grabbed_trains[next_grab_id].is_valid) {
		increment_next_grab_id();
		while (grabbed_trains[next_grab_id].is_valid) {
			if (next_grab_id == start) {
				pthread_mutex_unlock(&grabbed_trains_mutex);
				syslog_server(LOG_ERR, 
				              "Grab train - train: %s engine: %s - all grab-ids in use", 
				              train, engine);
				return -2;
			}
			increment_next_grab_id();
		}
	}
	// Assign grab-id, set track output to master, set engine instance
	const int grab_id = next_grab_id;
	increment_next_grab_id(); // increment for next "grab" action
	grabbed_trains[grab_id].name = g_string_new(train);
	
	strcpy(grabbed_trains[grab_id].track_output, "master");
	
	if (dyn_containers_set_train_engine_instance(&grabbed_trains[grab_id], train, engine)) {
		pthread_mutex_unlock(&grabbed_trains_mutex);
		syslog_server(LOG_ERR, 
		              "Grab train - train: %s engine: %s - train engine could not be set", 
		              train, engine);
		return -1;
	}
	grabbed_trains[grab_id].is_valid = true;
	pthread_mutex_unlock(&grabbed_trains_mutex);
	syslog_server(LOG_NOTICE, 
	              "Grab train - train: %s engine: %s - train grabbed with id: %d", 
	              train, engine, grab_id);
	return grab_id;
}

bool release_train(int grab_id) {
	bool success = false;
	pthread_mutex_lock(&grabbed_trains_mutex);
	if (grabbed_trains[grab_id].is_valid) {
		// Release any route currently granted to the train.
		pthread_mutex_lock(&interlocker_mutex);
		const char *route_id = 
				interlocking_table_get_route_id_of_train(grabbed_trains[grab_id].name->str);
		pthread_mutex_unlock(&interlocker_mutex);
		if (route_id != NULL) {
			syslog_server(LOG_INFO, 
			              "Release train - grab-id: %d train: %s - releasing the "
			              "route (%s) granted to the train", 
			              grab_id, grabbed_trains[grab_id].name->str, route_id);
			release_route(route_id);
		}
		
		grabbed_trains[grab_id].is_valid = false;
		dyn_containers_free_train_engine_instance(grabbed_trains[grab_id].dyn_containers_engine_instance);
		syslog_server(LOG_NOTICE, 
		              "Release train - grab-id: %d train: %s - released", 
		              grab_id, grabbed_trains[grab_id].name->str);
		g_string_free(grabbed_trains[grab_id].name, true);
		grabbed_trains[grab_id].name = NULL;
		success = true;
	}
	pthread_mutex_unlock(&grabbed_trains_mutex);
	return success;
}

void release_all_grabbed_trains(void) {
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		release_train(i);
	}
}

bool set_dcc_speed_for_train_maybe_grabbed(const char *train_id, int speed, bool req_forwards, const char *track_output) {
	if (train_id == NULL || speed < 0 || speed > 126) {
		return false;
	}
	int grab_id = train_get_grab_id(train_id);
	if (grab_id != -1) {
		pthread_mutex_lock(&grabbed_trains_mutex);
		// Train is grabbed -> set speed via dynamic container
		const int engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
		if (track_output != NULL && strlen(track_output) <= 32) {
			strcpy(grabbed_trains[grab_id].track_output, track_output);
		}
		dyn_containers_set_train_engine_instance_inputs(engine_instance, speed, req_forwards);
		pthread_mutex_unlock(&grabbed_trains_mutex);
	} else {
		speed = req_forwards ? speed : (speed * -1);
		if (bidib_set_train_speed(train_id, speed, (track_output != NULL ? track_output : "master")) == 0) {
			bidib_flush();
		} else {
			return false;
		}
	}
	return true;
}

bool set_dcc_speed_for_grabbed_train(int grab_id, int speed, bool req_forwards, const char *track_output) {
	if (speed < 0 || speed > 126) {
		return false;
	} else if (grab_id >= 0 && grab_id < TRAIN_ENGINE_INSTANCE_COUNT_MAX && grabbed_trains[grab_id].is_valid) {
		// Train is grabbed -> set speed via dynamic container
		const int engine_instance = grabbed_trains[grab_id].dyn_containers_engine_instance;
		if (track_output != NULL && strlen(track_output) <= 32) {
			strcpy(grabbed_trains[grab_id].track_output, track_output);
		}
		dyn_containers_set_train_engine_instance_inputs(engine_instance, speed, req_forwards);
		return true;
	}
	return false;
}

void stop_all_trains() {
	t_bidib_id_list_query train_ids_query = bidib_get_trains();
	for (size_t i = 0; i < train_ids_query.length; i++) {
		const char *train_id = train_ids_query.ids[i];
		set_dcc_speed_for_train_maybe_grabbed(train_id, 0, true, NULL);
	}
	bidib_free_id_list_query(train_ids_query);
}

char *train_id_from_grab_id(int grab_id) {
	pthread_mutex_lock(&grabbed_trains_mutex);
	if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
		pthread_mutex_unlock(&grabbed_trains_mutex);
		return NULL;
	}
	if (grabbed_trains[grab_id].name == NULL) {
		pthread_mutex_unlock(&grabbed_trains_mutex);
		syslog_server(LOG_ERR, 
		              "Train id from grab-id - train with id %d marked valid but name is NULL", 
		              grab_id);
		return NULL;
	}
	char *train_id = strdup(grabbed_trains[grab_id].name->str);
	pthread_mutex_unlock(&grabbed_trains_mutex);
	if (train_id == NULL) {
		syslog_server(LOG_ERR, "Train id from grab-id - unable to allocate memory for train_id");
	}
	return train_id;
}

static GString *build_grab_fdbk_json(int l_session_id, int grab_id) {
	GString *g_feedback = g_string_sized_new(96);
	g_string_assign(g_feedback, "");
	append_start_of_obj(g_feedback, false);
	append_field_int_value(g_feedback, "session-id", l_session_id, true);
	append_field_int_value(g_feedback, "grab-id", grab_id, false);
	append_end_of_obj(g_feedback, false);
	return g_feedback;
}

o_con_status handler_grab_train(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const char *data_engine = onion_request_get_post(req, "engine");
		
		if (handle_param_miss_check(res, "Grab train", "train", data_train)
			|| handle_param_miss_check(res, "Grab train", "engine", data_engine)) {
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Grab train - train: %s engine: %s - start", 
		              data_train, data_engine);
		
		t_bidib_train_state_query train_state_query = bidib_get_train_state(data_train);
		bool trainstate_known = train_state_query.known;
		bidib_free_train_state_query(train_state_query);
		if (!trainstate_known) {
			send_common_feedback(res, HTTP_NOT_FOUND, "unknown train or invalid train state");
			syslog_server(LOG_ERR, 
			              "Request: Grab train - train: %s engine: %s - "
			              "unknown train or invalid train state - abort", 
			              data_train, data_engine);
			return OCS_PROCESSED;
		}
		
		int grab_id = grab_train(data_train, data_engine);
		// No extra syslog per case as grab_train logs extensively.
		if (grab_id >= 0) {
			send_some_gstring_and_free(res, HTTP_OK, build_grab_fdbk_json(session_id, grab_id));
		} else if (grab_id == -4) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameters");
		} else if (grab_id == -3) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "train has already been grabbed");
		} else if (grab_id == -2) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "all grab-IDs are in use (max no. of grabbed trains reached)");
		} else if (grab_id == -1) {
			send_common_feedback(res, HTTP_INTERNAL_ERROR, 
			                     "internal err - failed to start train engine container");
		} else {
			send_common_feedback(res, HTTP_INTERNAL_ERROR, 
			                     "internal err - unexpected error case in grab_train");
		}
		syslog_server(LOG_NOTICE, 
		              "Request: Grab train - train: %s engine: %s - finish", 
		              data_train, data_engine);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Grab train");
	}
}

o_con_status handler_release_train(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		
		if (handle_param_miss_check(res, "Release train", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Release train", "grab-id", data_grab_id)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Release train - grab-id: %d - invalid session-id: %s", 
			              grab_id, data_session_id);
			return OCS_PROCESSED;
		}
		
		// If grab_id is valid, train_id will be, too.
		char *train_id = train_id_from_grab_id(grab_id);
		if (train_id == NULL) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, "Request: Release train - grab-id: %d - invalid grab-id", grab_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Release train - grab-id: %d train: %s - start", 
		              grab_id, train_id);
		
		// Set train speed to 0
		set_dcc_speed_for_train_maybe_grabbed(train_id, 0, true, NULL);
		
		// Wait until the train has stopped moving
		///NOTE: There is a potential race condition with set-dcc-speed:
		///      If release is requested and a non-zero speed is set just barely after/during that, 
		///      the effective speed will not become 0 (which we wait for in the loop below).
		///TODO: Consider adding a timeout after which the train is directly set to stop via bidib.
		t_bidib_train_state_query train_state_query = bidib_get_train_state(train_id);
		while (train_state_query.data.set_speed_step != 0) {
			bidib_free_train_state_query(train_state_query);
			usleep(TRAIN_DRIVE_TIME_STEP);
			train_state_query = bidib_get_train_state(train_id);
		}
		bidib_free_train_state_query(train_state_query);
		
		if (!release_train(grab_id)) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "train is not currently grabbed");
			syslog_server(LOG_ERR, 
			              "Request: Release train - grab-id: %d train: %s - "
			              "train is not currently grabbed - abort", 
			              grab_id, train_id);
		} else {
			onion_response_set_code(res, HTTP_OK);
			syslog_server(LOG_NOTICE, 
			              "Request: Release train - grab-id: %d train: %s - finish", 
			              grab_id, train_id);
		}
		free(train_id);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Release train");
	}
}

static void process_request_route_and_reply(onion_response *res, const char *train_id, 
                                            const char *source, const char *destination) {
	syslog_server(LOG_NOTICE, 
	              "Request: Request route - train: %s from: %s to: %s - start", 
	              train_id, source, destination);
	// Use interlocker to find and grant a route
	GString *route_id = grant_route(train_id, source, destination);
	// No extra syslog in both branches as grant_route logs extensively
	if (route_id != NULL && route_id->str != NULL && params_check_is_number(route_id->str)) {
		send_single_str_field_feedback(res, HTTP_OK, "granted-route-id", route_id->str);
	} else if (route_id == NULL || route_id->str == NULL) {
		send_common_feedback(res, HTTP_BAD_REQUEST, "Route could not be granted");
	} else if (strcmp(route_id->str, "no_interlocker") == 0) {
		send_common_feedback(res, HTTP_BAD_REQUEST, "No interlocker selected for use");
	} else if (strcmp(route_id->str, "no_routes") == 0) {
		send_common_feedback(res, HTTP_BAD_REQUEST, "No routes possible");
	} else if (strcmp(route_id->str, "not_grantable") == 0) {
		send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
		                     "Route conflicts with granted route(s)");
	} else if (strcmp(route_id->str, "not_clear") == 0) {
		send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
		                     "Route found has occupied tracks or source signal is not stop");
	} else {
		send_common_feedback(res, HTTP_BAD_REQUEST, "Route could not be granted");
	}
	syslog_server(LOG_NOTICE, 
	              "Request: Request route - train: %s from: %s to: %s - finish",
	              train_id, source, destination);
	g_string_free(route_id, true);
}

o_con_status handler_request_route(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_source_name = onion_request_get_post(req, "source");
		const char *data_destination_name = onion_request_get_post(req, "destination");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		
		if (handle_param_miss_check(res, "Request route", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Request route", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Request route", "destination", data_destination_name)
			|| handle_param_miss_check(res, "Request route", "source", data_source_name)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Request route - from: %s to: %s - invalid session-id (%s)", 
			              data_source_name, data_destination_name, data_session_id);
			return OCS_PROCESSED;
		}
		// If grab_id is valid, train_id will be, too.
		char *train_id = train_id_from_grab_id(grab_id);
		if (train_id == NULL) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Request route - from: %s to: %s - invalid grab-id (%s)",
			              data_source_name, data_destination_name, data_grab_id);
			return OCS_PROCESSED;
		}
		
		process_request_route_and_reply(res, train_id, data_source_name, data_destination_name);
		free(train_id);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Request route");
	}
}

o_con_status handler_request_route_by_id(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const char *route_id = params_check_route_id(data_route_id);
		
		if (handle_param_miss_check(res, "Request route by id", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Request route by id", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Request route by id", "route-id", data_route_id)) {
			return OCS_PROCESSED;
		} else if (strcmp(route_id, "") == 0) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid route-id");
			syslog_server(LOG_ERR, 
			              "Request: Request route by id - invalid route-id (%s)", 
			              data_route_id);
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Request route by id - route: %s - invalid session-id (%s)", 
			              route_id, data_session_id);
			return OCS_PROCESSED;
		}
		// If grab_id is valid, train_id will be, too.
		char *train_id = train_id_from_grab_id(grab_id);
		if (train_id == NULL) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Request route by id - route: %s - invalid grab-id (%s)", 
			              route_id, data_grab_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Request route by id - route: %s train: %s - start",
		              route_id, train_id);
		
		// Grant the route ID using an internal algorithm
		const char *result = grant_route_id(train_id, route_id);
		// No extra syslog as grant_route_id logs extensively
		if (strcmp(result, "granted") == 0) {
			onion_response_set_code(res, HTTP_OK);
		} else if (strcmp(result, "not_known") == 0) {
			send_common_feedback(res, HTTP_NOT_FOUND, "Route is not known");
		} else if (strcmp(result, "already_granted") == 0) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "Route is already granted to another train");
		} else if (strcmp(result, "not_grantable") == 0) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "Route not available or has conflicts with granted route(s)");
		} else if (strcmp(result, "not_clear") == 0) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, 
			                     "Route has occupied tracks or source signal is not stop");
		} else if (strcmp(result, "internal_error") == 0) {
			send_common_feedback(res, HTTP_INTERNAL_ERROR, 
			                     "Route granting failed due to internal error");
		} else {
			send_common_feedback(res, HTTP_BAD_REQUEST, "Route could not be granted");
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Request route by id - route: %s train: %s - finish",
		              route_id, train_id);
		free(train_id);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Request route by id");
	}
}

o_con_status handler_driving_direction(void *_, onion_request *req, onion_response *res) {
	// Notes regarding documentation:
	// The driving direction is determined based on the route specified *and* the trains position 
	// and the trains orientation. The position of the train is relevant as a Kehrschleife/
	// reverser can influence the expected/correct result.
	// I.e., given where the train currently is located, and which route is to be driven,
	// what direction would the train have to drive (forwards/backwards) to reach the destination?
	
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		
		if (handle_param_miss_check(res, "Driving direction", "train", data_train)
			|| handle_param_miss_check(res, "Driving direction", "route-id", data_route_id)) {
			return OCS_PROCESSED;
		} else if (strcmp(route_id, "") == 0) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid route-id");
			syslog_server(LOG_ERR, 
			              "Request: Driving direction - train: %s - invalid route-id (%s)", 
			              data_train, data_route_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_INFO, 
		              "Request: Driving direction - train: %s route: %s - start", 
		              data_train, route_id);
		pthread_mutex_lock(&interlocker_mutex);
		const t_interlocking_route *route = get_route(route_id);
		if (route == NULL) {
			send_common_feedback(res, HTTP_NOT_FOUND, "no route with given route-id known");
			syslog_server(LOG_INFO, 
			              "Request: Driving direction - train: %s route: %s - unknown route", 
			              data_train, route_id);
		} else if (is_forward_driving(route, data_train)) {
			send_some_cstring(res, HTTP_OK, "{\"direction\": \"forwards\"}");
		} else {
			send_some_cstring(res, HTTP_OK, "{\"direction\": \"backwards\"}");
		}
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_INFO, 
		              "Request: Driving direction - train: %s route: %s - finish", 
		              data_train, route_id);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Driving direction");
	}
}

o_con_status handler_drive_route(void *_, onion_request *req, onion_response *res) {
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
		
		if (handle_param_miss_check(res, "Drive route", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Drive route", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Drive route", "route-id", data_route_id)
			|| handle_param_miss_check(res, "Drive route", "mode", data_mode)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, "Request: Drive route - invalid session-id (%s)", data_session_id);
			return OCS_PROCESSED;
		} else if (strcmp(mode, "") == 0) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid driving mode");
			syslog_server(LOG_ERR, "Request: Drive route - invalid driving mode (%s)", data_mode);
			return OCS_PROCESSED;
		} else if (strcmp(route_id, "") == 0) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid route-id");
			syslog_server(LOG_ERR, "Request: Drive route - invalid route-id (%s)", data_route_id);
			return OCS_PROCESSED;
		}
		
		// If grab_id is valid, train_id will be, too.
		char *train_id = train_id_from_grab_id(grab_id);
		if (train_id == NULL) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Drive route - route: %s - invalid grab-id (%s)", 
			              route_id, data_grab_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Drive route - route: %s train: %s drive mode: %s - start", 
		              route_id, train_id, mode);
		
		const bool is_automatic = (strcmp(mode, "automatic") == 0);
		if (drive_route(train_id, route_id, is_automatic)) {
			send_common_feedback(res, HTTP_OK, "Route driving completed");
			syslog_server(LOG_NOTICE, 
			              "Request: Drive route - route: %s train: %s drive mode: %s - finish", 
			              route_id, train_id, mode);
		} else {
			send_common_feedback(res, HTTP_BAD_REQUEST, 
			                     "Route driving failed; please ensure that the route is granted "
			                     "to the train corresponding to the grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Drive route - route: %s train: %s drive mode: %s - "
			              "driving failed - abort", 
			              route_id, train_id, mode);
		}
		free(train_id);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Drive route");
	}
}

o_con_status handler_set_dcc_train_speed(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_speed = onion_request_get_post(req, "speed");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const int speed = params_check_speed(data_speed);
		
		if (handle_param_miss_check(res, "Set dcc train speed", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Set dcc train speed", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Set dcc train speed", "track-output", data_track_output)
			|| handle_param_miss_check(res, "Set dcc train speed", "speed", data_speed)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Set dcc train speed - invalid session-id (%s)", 
			              data_session_id);
			return OCS_PROCESSED;
		}
		
		pthread_mutex_lock(&grabbed_trains_mutex);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Set dcc train speed - invalid grab-id (%s)", 
			              data_grab_id);
			return OCS_PROCESSED;
		} else if (speed == 999) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "bad speed");
			syslog_server(LOG_ERR, 
			              "Request: Set dcc train speed - train: %s speed: %d - bad speed (%s)",
			              grabbed_trains[grab_id].name->str, speed, data_speed);
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		} else if (strlen(data_track_output) > 32) {
			// strlen check here as the value is copied to grabbed_trains[grab_id].track_output,
			// which has a length of 32.
			send_common_feedback(res, HTTP_BAD_REQUEST, 
			                     "invalid track output (currently no track output longer than "
			                     "32 chars, incl. null terminator, is supported for this endpoint)");
			syslog_server(LOG_ERR, 
			              "Request: Set dcc train speed - train: %s speed: %d - "
			              "invalid track output (%s)",
			              grabbed_trains[grab_id].name->str, speed, data_track_output);
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set dcc train speed - train: %s speed: %d - start",
		              grabbed_trains[grab_id].name->str, speed);
		set_dcc_speed_for_grabbed_train(grab_id, abs(speed), speed >= 0, data_track_output);
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set dcc train speed - train: %s speed: %d - finish",
		              grabbed_trains[grab_id].name->str, speed);
		pthread_mutex_unlock(&grabbed_trains_mutex);
		onion_response_set_code(res, HTTP_OK);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set dcc train speed");
	}
}

o_con_status handler_set_calibrated_train_speed(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_speed = onion_request_get_post(req, "speed");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const int speed = params_check_calibrated_speed(data_speed);
		
		if (handle_param_miss_check(res, "Set calibrated train speed", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Set calibrated train speed", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Set calibrated train speed", "track-output", 
			                           data_track_output)
			|| handle_param_miss_check(res, "Set calibrated train speed", "speed", data_speed)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Set calibrated train speed - invalid session-id (%s)", 
			              data_session_id);
			return OCS_PROCESSED;
		} 
		
		pthread_mutex_lock(&grabbed_trains_mutex);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Set calibrated train speed - invalid grab-id (%s)", 
			              data_grab_id);
			return OCS_PROCESSED;
		} else if (speed == 999) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "bad speed");
			syslog_server(LOG_ERR, 
			              "Request: Set calibrated train speed - train: %s speed: %d - bad speed (%s)", 
			              grabbed_trains[grab_id].name->str, speed, data_speed);
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set calibrated train speed - train: %s speed: %d - start",
		              grabbed_trains[grab_id].name->str, speed);
		if (bidib_set_calibrated_train_speed(grabbed_trains[grab_id].name->str,
		                                     speed, data_track_output)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set calibrated train speed - train: %s speed: %d - "
			              "invalid parameter values - abort", 
			              grabbed_trains[grab_id].name->str, speed);
		} else {
			bidib_flush();
			onion_response_set_code(res, HTTP_OK);
			syslog_server(LOG_NOTICE, 
			              "Request: Set calibrated train speed - train: %s speed: %d - finish",
			              grabbed_trains[grab_id].name->str, speed);
		}
		pthread_mutex_unlock(&grabbed_trains_mutex);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set calibrated train speed");
	}
}

o_con_status handler_set_train_emergency_stop(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const int client_session_id = params_check_session_id(data_session_id);
		
		if (handle_param_miss_check(res, "Set train emergency stop", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Set train emergency stop", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Set train emergency stop", "track-output", 
			                           data_track_output)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Set train emergency stop - invalid session-id (%s)", 
			              data_session_id);
			return OCS_PROCESSED;
		}
		
		pthread_mutex_lock(&grabbed_trains_mutex);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Set train emergency stop - invalid grab-id (%s)", 
			              data_grab_id);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set train emergency stop - train: %s - start",
		              grabbed_trains[grab_id].name->str);
		
		if (bidib_emergency_stop_train(grabbed_trains[grab_id].name->str, data_track_output)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set train emergency stop - train: %s - "
			              "invalid parameter values - abort", 
			              grabbed_trains[grab_id].name->str);
		} else {
			bidib_flush();
			onion_response_set_code(res, HTTP_OK);
			syslog_server(LOG_NOTICE, 
			              "Request: Set train emergency stop - train: %s - finish",
			              grabbed_trains[grab_id].name->str);
		}
		pthread_mutex_unlock(&grabbed_trains_mutex);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set train emergency stop");
	}
}

o_con_status handler_set_train_peripheral(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_session_id = onion_request_get_post(req, "session-id");
		const char *data_grab_id = onion_request_get_post(req, "grab-id");
		const char *data_peripheral = onion_request_get_post(req, "peripheral");
		const char *data_state = onion_request_get_post(req, "state");
		const char *data_track_output = onion_request_get_post(req, "track-output");
		const int client_session_id = params_check_session_id(data_session_id);
		const int grab_id = params_check_grab_id(data_grab_id, TRAIN_ENGINE_INSTANCE_COUNT_MAX);
		const int state = params_check_state(data_state);
		
		if (handle_param_miss_check(res, "Set train peripheral", "session-id", data_session_id)
			|| handle_param_miss_check(res, "Set train peripheral", "grab-id", data_grab_id)
			|| handle_param_miss_check(res, "Set train peripheral", "peripheral", data_peripheral)
			|| handle_param_miss_check(res, "Set train peripheral", "track-output", data_track_output)
			|| handle_param_miss_check(res, "Set train peripheral", "state", data_state)) {
			return OCS_PROCESSED;
		} else if (client_session_id != session_id) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid session-id");
			syslog_server(LOG_ERR, 
			              "Request: Set train peripheral - invalid session-id (%s)", 
			              data_session_id);
			return OCS_PROCESSED;
		} 
		
		pthread_mutex_lock(&grabbed_trains_mutex);
		if (grab_id == -1 || !grabbed_trains[grab_id].is_valid) {
			pthread_mutex_unlock(&grabbed_trains_mutex);
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid grab-id");
			syslog_server(LOG_ERR, 
			              "Request: Set train peripheral - invalid grab-id (%s)", 
			              data_grab_id);
			return OCS_PROCESSED;
		} else if (state == -1) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid state");
			syslog_server(LOG_ERR, 
			              "Request: Set train peripheral - train: %s - invalid state (%s)", 
			              grabbed_trains[grab_id].name->str, data_state);
			pthread_mutex_unlock(&grabbed_trains_mutex);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Set train peripheral - train: %s peripheral: %s state: %d - start", 
		              grabbed_trains[grab_id].name->str, data_peripheral, state);
		if (bidib_set_train_peripheral(grabbed_trains[grab_id].name->str,
		                               data_peripheral, state,
		                               data_track_output)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "invalid parameter values");
			syslog_server(LOG_ERR, 
			              "Request: Set train peripheral - train: %s "
			              "peripheral: %s state: %d - invalid parameter values - abort",
			              grabbed_trains[grab_id].name->str, data_peripheral, state);
		} else {
			bidib_flush();
			onion_response_set_code(res, HTTP_OK);
			syslog_server(LOG_NOTICE, 
			              "Request: Set train peripheral - train: %s peripheral: %s state: %d"
			              " - finish",
			              grabbed_trains[grab_id].name->str, data_peripheral, state);
		}
		pthread_mutex_unlock(&grabbed_trains_mutex);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Set train peripheral");
	}
}

