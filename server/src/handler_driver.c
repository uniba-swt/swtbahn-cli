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

#include <onion/onion.h>
#include <bidib/bidib.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

#include "server.h"
#include "dyn_containers_interface.h"
#include "handler_controller.h"
#include "interlocking.h"
#include "param_verification.h"
#include "bahn_data_util.h"

#define MICROSECOND 1
#define TRAIN_DRIVE_TIME_STEP 	10000 * MICROSECOND		// 0.01 seconds

pthread_mutex_t grabbed_trains_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int DRIVING_SPEED_SLOW = 40;
static unsigned int next_grab_id = 0;


typedef struct {
	char *id;
	bool has_been_set_to_stop;
	bool is_source_signal;
	bool is_destination_signal;
	size_t index_in_route_path;
} t_route_signal_info;

typedef struct {
	t_route_signal_info **data_ptr;
	size_t len;
} t_route_signal_info_array;

t_train_data grabbed_trains[TRAIN_ENGINE_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_engine_instance = -1 }
};

typedef struct {
	size_t *arr;
	size_t len;
} t_route_repeated_segment_indices;


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

static bool validate_interlocking_route_members_not_null(const t_interlocking_route *route) {
	if (route == NULL) {
		return false;
	}
	return (route->conflicts != NULL && route->destination != NULL && route->id != NULL 
	        && route->orientation != NULL && route->path != NULL && route->points != NULL 
	        && route->sections != NULL && route->signals != NULL && route->source != NULL
	        && route->train != NULL);
}


static void log_signal_info(int priority, const t_route_signal_info *sig_info) {
	if (sig_info != NULL) {
		syslog_server(priority, "\tID %s", (sig_info->id != NULL ? sig_info->id : "NULL") );
		syslog_server(priority, "\tis source?      %s", sig_info->is_source_signal ? "yes" : "no");
		syslog_server(priority, "\tis destination? %s", sig_info->is_destination_signal ? "yes" : "no");
		syslog_server(priority, "\tindex in path   %d", sig_info->index_in_route_path);
	}
}

static void log_signal_info_array(int priority, const t_route_signal_info_array *sig_info_array) {
	syslog_server(priority, "Drive route signal info array: Begin");
	if (sig_info_array != NULL) {
		for (size_t i = 0; i < sig_info_array->len; ++i) {
			log_signal_info(priority, sig_info_array->data_ptr[i]);
		}
	}
	syslog_server(priority, "Drive route signal info array: End");
}

static void free_route_signal_info_array(t_route_signal_info_array *route_signal_info_array) {
	if (route_signal_info_array == NULL) {
		return;
	}
	if (route_signal_info_array->data_ptr == NULL) {
		route_signal_info_array->len = 0;
		return;
	}
	for (size_t i = 0; i < route_signal_info_array->len; ++i) {
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

static t_route_signal_info_array get_route_signal_info_array(const t_interlocking_route *route) {
	// Strategy:
	// 1. Allocate memory for array of pointers to t_route_signal_info type entities
	// 2. For every signal in route->signals...
	//   A. Skip iteration if signal from route->signals is NULL
	//   B. Allocate memory for member t_route_signal_info at position i in info_arr
	//   C. Set t's trivial members to appropriate defaults (set to stop, is source, is destination)
	//   D. Copy ID from signal (from route->signals) to new signal_info.
	// 3. For every signal_info member in info_arr, determine index of that signal-id 
	//    in route->path and set member for index_in_route_path of signal_info accordingly
	
	t_route_signal_info_array info_arr = {.data_ptr = NULL, .len = 0};
	if (!validate_interlocking_route_members_not_null(route)) {
		syslog_server(LOG_ERR, 
		             "Get drive route signal info array: route is NULL or some route details are NULL");
		return info_arr;
	}
	syslog_server(LOG_NOTICE, "Get drive route signal info array: Start building for route id %s", route->id);
	
	// 1. Allocate memory for array of pointers to t_route_signal_info type entities
	const size_t number_of_signal_infos = route->signals->len;
	info_arr.data_ptr = (t_route_signal_info**) malloc(sizeof(t_route_signal_info*) * number_of_signal_infos);
	
	if (info_arr.data_ptr == NULL) {
		syslog_server(LOG_ERR, "Get drive route signal info array: Could not allocate memory for array");
		return info_arr;
	}
	// 2. For every signal in route->signals...
	for (size_t i = 0; i < number_of_signal_infos; ++i) {
		const char *signal_id_item = g_array_index(route->signals, char *, i);
		
		// A. Skip iteration if signal from route->signals is NULL
		if (signal_id_item == NULL) {
			syslog_server(LOG_WARNING,
			              "Get drive route signal info array: "
			              "Skipping NULL signal at index %d of route->signals", i);
			info_arr.data_ptr[i] = NULL;
			continue;
		}
		// B. Allocate memory for member t_route_signal_info at position i in info_arr.
		info_arr.data_ptr[i] = (t_route_signal_info *) malloc(sizeof(t_route_signal_info));
		info_arr.len = i + 1;
		if (info_arr.data_ptr[i] == NULL) {
			syslog_server(LOG_ERR, 
			              "Get drive route signal info array: "
			              "Unable to allocate memory for array index %d", i);
			free_route_signal_info_array(&info_arr);
			return info_arr;
		}
		
		// C. Set t's trivial members to appropriate defaults
		info_arr.data_ptr[i]->has_been_set_to_stop = false;
		info_arr.data_ptr[i]->is_source_signal = (i == 0);
		info_arr.data_ptr[i]->is_destination_signal = (i+1 == number_of_signal_infos);
		
		// D. Copy ID from signal (from route->signals) to new signal_info.
		info_arr.data_ptr[i]->id = strdup(signal_id_item);
		if (info_arr.data_ptr[i]->id == NULL) {
			syslog_server(LOG_ERR,
			              "Get drive route signal info array: Unable to allocate memory for signal id %s",
			              signal_id_item);
			free_route_signal_info_array(&info_arr);
			return info_arr;
		}
	}
	// 3. For every signal_info, determine index of that signal-id in route->path and set member 
	//    for index_in_route_path of signal_info accordingly
	size_t path_index = 0;
	for (size_t i = 0; i < info_arr.len; ++i) {
		if (info_arr.data_ptr[i] == NULL) {
			syslog_server(LOG_WARNING, 
			              "Get drive route signal info array: "
			              "Skipping NULL signal_info at index %d of info_arr", i);
			continue;
		}
		
		info_arr.data_ptr[i]->index_in_route_path = 0;
		if (info_arr.data_ptr[i]->is_source_signal || info_arr.data_ptr[i]->is_destination_signal) {
			continue;
		}
		// path_index not reset on purpose, as info_arr signals are ordered ascendingly
		// in relation to their occurrence in the route. That is, a signal at position 5 in info_arr
		// must have a higher index_in_route_path than a signal at position < 5 in info_arr.
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


void free_route_repeated_segment_indices(t_route_repeated_segment_indices *r_seg_indices) {
	if (r_seg_indices != NULL && r_seg_indices->arr != NULL) {
		free(r_seg_indices->arr);
		r_seg_indices->arr = NULL;
		r_seg_indices->len = 0;
	}
}

// "repeated segment index" = Index of a segment that appears more than once in a route
static t_route_repeated_segment_indices get_route_repeated_segment_indices(const t_interlocking_route *route) {
	// For route, determine which segments appear more than once
	t_route_repeated_segment_indices r_seg_indices = {.arr = NULL, .len = 0};
	if (route == NULL || route->path == NULL) {
		return r_seg_indices;
	}
	// First allocate enough memory for more than the maximum possible amount of 
	// duplicate segment indices. Will afterwards be reallocated to the size it actually needs.
	r_seg_indices.arr = malloc(sizeof(size_t) * route->path->len);
	if (r_seg_indices.arr == NULL) {
		syslog_server(LOG_ERR, 
		              "Get repeated segment indices for route %s: Unable to allocate memory for array",
		              route->id);
		return r_seg_indices;
	}
	// For every segment in route->path, check if it occurs again at a different position. 
	// If yes, add the occurrence (from outer loop) to r_seg_indices and break the inner loop.
	for (size_t path_index_i = 0; path_index_i < route->path->len; ++path_index_i) {
		const char *path_item_i = g_array_index(route->path, char *, path_index_i);
		if (path_item_i != NULL && is_type_segment(path_item_i)) {
			for (size_t path_index_n = 0; path_index_n < route->path->len; ++path_index_n) {
				const char *path_item_n = g_array_index(route->path, char *, path_index_n);
				if (path_item_n != NULL && strcmp(path_item_i, path_item_n) == 0
				        && path_index_i != path_index_n) {
					r_seg_indices.arr[r_seg_indices.len] = path_index_i;
					r_seg_indices.len++;
					break;
				}
			}
		}
	}
	if (r_seg_indices.len == 0) {
		free(r_seg_indices.arr);
		r_seg_indices.arr = NULL;
	} else {
		size_t *tmp = reallocarray(r_seg_indices.arr, r_seg_indices.len, sizeof(size_t));
		if (tmp == NULL) {
			syslog_server(LOG_ERR, 
			              "Get repeated segment indices for route %s: Unable to re-allocate memory", 
			              route->id);
			free(r_seg_indices.arr);
			r_seg_indices.arr = NULL;
			r_seg_indices.len = 0;
		} else {
			// re-alloc successful
			r_seg_indices.arr = tmp;
		}
	}
	return r_seg_indices;
}

// Queries the position of the train. Checks which segments that the train occupies exist in the route path. 
// Return -3 on param err, -2 on train not on tracks, -1 on train not on route, >= 0 if train is on route.
// If train occupies at least one segment in route path, returns the route-path index of the occupied segment
// that is the furthest along the route. Ignores any segments that appear more than once in route->path
ssize_t get_train_pos_index_in_route_ignore_repeated_segments(const char *train_id, const t_interlocking_route *route, 
                                                              const t_route_repeated_segment_indices *repeated_segment_indices) {
	if (train_id == NULL || route == NULL || route->path == NULL || repeated_segment_indices == NULL) {
		return -3;
	}
	t_bidib_train_position_query train_pos_query = bidib_get_train_position(train_id);
	if (train_pos_query.segments == NULL || train_pos_query.length == 0) {
		bidib_free_train_position_query(train_pos_query);
		return -2;
	}
	ssize_t train_pos_index = -1;
	size_t train_pos_index_unsigned = 0;
	const size_t path_count = route->path->len;
	for (size_t i = 0; i < train_pos_query.length; ++i) {
		// Search starting at most recent pos_index to skip unnecessary comparisons
		// If index is present in array of multi-occurring segments, skip iteration 
		for (size_t n = train_pos_index_unsigned; n < path_count; ++n) {
			bool ignore = false;
			for (size_t k = 0; k < repeated_segment_indices->len; ++k) {
				if (repeated_segment_indices->arr[k] == n) {
					ignore = true;
					break;
				}
			}
			if (!ignore) {
				const char *path_item = g_array_index(route->path, char *, n);
				if (n > train_pos_index_unsigned && strcmp(path_item, train_pos_query.segments[i]) == 0) {
					train_pos_index = (ssize_t) n;
					train_pos_index_unsigned = n;
					break;
				}
			}
		}
	}
	bidib_free_train_position_query(train_pos_query);
	return train_pos_index;
}

// Queries the position of the train. Checks which segments that the train occupies exist in the route path. 
// Return -3 on param err, -2 on train not on tracks, -1 on train not on route, >= 0 if train is on route.
// If train occupies at least one segment in route path, returns the route-path index of the occupied segment
// that is the furthest along the route.
ssize_t get_train_pos_index_in_route_path(const char *train_id,  const t_interlocking_route *route) {
	if (train_id == NULL || route == NULL || route->path == NULL) {
		return -3;
	}
	t_bidib_train_position_query train_position_query = bidib_get_train_position(train_id);
	if (train_position_query.segments == NULL || train_position_query.length == 0) {
		bidib_free_train_position_query(train_position_query);
		return -2;
	}
	ssize_t train_pos_index = -1;
	size_t train_pos_index_unsigned = 0;
	const size_t path_count = route->path->len;
	for (size_t i = 0; i < train_position_query.length; ++i) {
		// Search starting at most recent pos_index to skip unnecessary comparisons
		for (size_t n = train_pos_index_unsigned; n < path_count; ++n) {
			const char *path_item = g_array_index(route->path, char *, n);
			if (n > train_pos_index_unsigned && strcmp(path_item, train_position_query.segments[i]) == 0) {
				train_pos_index = (ssize_t) n;
				train_pos_index_unsigned = n;
				break;
			}
		}
	}
	bidib_free_train_position_query(train_position_query);
	return train_pos_index;
}

// For a train at position train_pos_index in route->path, set all signals to stop that
// the train has passed and have not yet been set to stop.
// Returns the count of how many signals have been set to stop in this function
static size_t update_route_signals_for_train_pos(t_route_signal_info_array *signal_infos, 
                                                 t_interlocking_route *route,
                                                 ssize_t train_pos_index) {
	size_t signals_set_to_stop = 0;
	const char *signal_stop_aspect = "aspect_stop";
	// 1. Determine which signals have a lower or equal index in route->path than the
	//    'train index' (meaning the train has passed them) and set them to aspect_stop.
	for (size_t sig_info_index = 0; sig_info_index < signal_infos->len; ++sig_info_index) {
		t_route_signal_info *sig_info_elem = signal_infos->data_ptr[sig_info_index];
		
		if (sig_info_elem == NULL) {
			syslog_server(LOG_ERR, "Drive-Route Decoupled: Signal in signal_infos at "
			              "index %d is NULL", sig_info_index);
			continue;
		}
		// 2. Determine if this signal is eligible to be set to stop
		if (!(sig_info_elem->has_been_set_to_stop) && !(sig_info_elem->is_destination_signal)) {
			
			// The following IF uses '<=' and not '<' because the source signal has a "fake" 
			// position index of 0 since it never appears in the actual route elements list,
			// and if train is on first segment of the route (pos index 0), it has passed
			// the source signal. With '<', the source signal would only be set to stop
			// once the train reaches the _second_ segment of the route.
			
			// 3. Determine if signal has been passed by the train
			if ((ssize_t) sig_info_elem->index_in_route_path <= train_pos_index) {
				// 4. Try to set the signal to signal_stop_aspect.
				if (bidib_set_signal(sig_info_elem->id, signal_stop_aspect)) {
					// Log_ERR... but what else? Safety violation once we have a safety layer?
					syslog_server(LOG_ERR, "Drive-Route Decoupled: unable to set signal %s to stop",
					              sig_info_elem->id);
				} else {
					bidib_flush();
					syslog_server(LOG_NOTICE, "Drive-Route Decoupled: Signal %s set to stop",
					              sig_info_elem->id);
					sig_info_elem->has_been_set_to_stop = true;
					signals_set_to_stop++;
				}
			}
		}
	}
	return signals_set_to_stop;
}


/**
 * @brief For a train driving a route, set the signals to stop that the train passes
 * 
 * @param train_id The train driving the route
 * @param route The route to be driven
 * @return true signal updating successful (all signals were passed and set to stop)
 * @return false signal updating failed
 */
static bool drive_route_progressive_stop_signals_decoupled(const char *train_id, t_interlocking_route *route) {
	if (route == NULL || route->id == NULL) {
		syslog_server(LOG_ERR, "Drive-Route Decoupled: route or route->id is NULL");
		return false;
	}
	if (train_id == NULL) {
		syslog_server(LOG_ERR, "Drive-Route Decoupled: train_id is NULL");
		return false;
	}
	
	// 1. Get route signal infos and indices of segments that appear more than once
	t_route_signal_info_array signal_infos = get_route_signal_info_array(route);
	t_route_repeated_segment_indices multi_segment_indices = get_route_repeated_segment_indices(route);
	
	// Check that signal_infos array is not empty and has at least 2 entries (source, destination)
	if (signal_infos.data_ptr == NULL) {
		syslog_server(LOG_ERR, "Drive-Route Decoupled: signal-infos array member null");
		free_route_signal_info_array(&signal_infos);
		free_route_repeated_segment_indices(&multi_segment_indices);
		return false;
	} else if (signal_infos.len < 2) {
		free_route_signal_info_array(&signal_infos);
		free_route_repeated_segment_indices(&multi_segment_indices);
		syslog_server(LOG_ERR, "Drive-Route Decoupled: "
		              "got signal-infos, but has not enough entries (< 2)");
		return false;
	}
	syslog_server(LOG_DEBUG, "Drive-Route Decoupled: got signal-infos "
	              "with arr length %d for routeID %s", signal_infos.len, route->id);
	
	// Signals in a route shall be set to stop once the train has driven past them.
	// The destination signal shall not be driven past, thus there
	// are (signal_infos.len - 1) signals to set to stop in total for driving this route
	const size_t signals_to_set_to_stop_count = signal_infos.len - 1;
	size_t signals_set_to_stop = 0;
	
	// Initialised to -3 as -2 and -1 are used for error return indication in the function that
	// determines the train's position
	ssize_t train_pos_index_previous = -3;
	
	// Main route driving loop runs as long as server is running, the drive route params are valid,
	// and not all signals have yet been set to stop
	while (running && drive_route_params_valid(train_id, route) 
	       && signals_set_to_stop < signals_to_set_to_stop_count) {
		// 1. Get position of train, and determine index (in route->path) of where the train is
		ssize_t train_pos_index = get_train_pos_index_in_route_ignore_repeated_segments(train_id, route, &multi_segment_indices);
		
		// 2. Check if the train_pos_index could not be determined either due to invalided params (-3)
		//    or due to the train not being on the tracks
		if (train_pos_index <= -2) {
			syslog_server(LOG_DEBUG, "Drive-Route Decoupled: unable to determine "
			              "the position of the train %s on the route %s", train_id, route->id);
			// Train position unknown -> skip this iteration. 
			// Don't want to stop route driving here as train might just be lost temporarily
			continue;
		}
		
		// 3. If train position has changed, check if any signals need to be set to aspect stop
		if (train_pos_index_previous != train_pos_index) {
			syslog_server(LOG_DEBUG, "Drive-Route Decoupled: Train position index now %d",
			              train_pos_index);
			if (train_pos_index_previous > train_pos_index) {
				syslog_server(LOG_DEBUG, 
				              "Drive-Route Decoupled: New train position index is lower than before",
				              train_pos_index);
			}
			signals_set_to_stop += update_route_signals_for_train_pos(&signal_infos, route, train_pos_index);
		}
		train_pos_index_previous = train_pos_index;
		usleep(TRAIN_DRIVE_TIME_STEP);
	}
	// NOTE: The "ending" does not mean that the end of the route was reached, but rather that
	//       there is no more need to set any route signals to stop.
	syslog_server(LOG_NOTICE, "Drive-Route Decoupled: for routeID %s ending; "
	              "%d signals were set to stop here in total.", route->id, signals_set_to_stop);
	free_route_signal_info_array(&signal_infos);
	free_route_repeated_segment_indices(&multi_segment_indices);
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
	if (train_id == NULL || route == NULL || !drive_route_params_valid(train_id, route)) {
		syslog_server(LOG_WARNING, "Drive route: Invalid params at start");
		free(train_id);
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
	const bool result = drive_route_progressive_stop_signals_decoupled(train_id, route);
	
	if (is_automatic && result) {
		const char *pre_dest_segment = g_array_index(route->path, char *, route->path->len - 2);
		while (running && result && !train_position_is_at(train_id, pre_dest_segment)
		                      && drive_route_params_valid(train_id, route)) {
			usleep(TRAIN_DRIVE_TIME_STEP);
			route = get_route(route->id);
		}
		pthread_mutex_lock(&grabbed_trains_mutex);
		syslog_server(LOG_NOTICE, "Drive route: Slowing %s for end of route %s", train_id, route_id);
		dyn_containers_set_train_engine_instance_inputs(engine_instance, 
		                                                (int) (DRIVING_SPEED_SLOW * 0.66),
		                                                 requested_forwards);
		pthread_mutex_unlock(&grabbed_trains_mutex);
	}
	// Wait for train to reach the end of the route
	const char *dest_segment = g_array_index(route->path, char *, route->path->len - 1);
	while (running && result && !is_segment_occupied(dest_segment) 
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

