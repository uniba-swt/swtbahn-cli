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

#include <unistd.h>
#include <onion/onion.h>
#include <bidib/bidib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "server.h"
#include "dyn_containers_interface.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"
#include "check_route_sectional/check_route_sectional.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MICROSECOND 1
static const int let_period_us = 100000 * MICROSECOND;	// 0.1 seconds

static t_interlocker_data interlocker_instances[INTERLOCKER_INSTANCE_COUNT_MAX] = {
	{ .is_valid = false, .dyn_containers_interlocker_instance = -1 }
};

static GString *selected_interlocker_name;
static int selected_interlocker_instance = -1;

static const char* block1[14] = {"signal5", "signal19", "signal45", "signal47", "signal49", "signal51", "signal9", "signal14", "signal24", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* block2[3] = {"signal8", "signal4a", "_end_"};
static const char* block3[14] =  {"signal7a", "signal22a", "signal35a", "signal46a", "signal48a", "signal50a", "signal53a", "signal1", "signal11", "signal15", "signal26", "signal28", "signal30", "_end_"};
static const char* block4[5] = {"signal1", "signal9", "signal15", "signal26", "_end_"};
static const char* block5[13] = {"signal9", "signal14", "signal24", "signal32", "signal37", "signal39", "signal41", "signal19", "signal45", "signal47", "signal49", "signal51", "_end_"};
static const char* block6[5] = {"signal4a", "signal18a", "signal21", "signal43", "_end_"};
static const char* block7[5] = {"signal8", "signal20", "signal23", "signal36", "_end_"};
static const char* block8and15[12] = {"signal22a", "signal35a", "signal46a", "signal48a", "signal50a", "signal53a", "signal1", "signal15", "signal26", "signal28", "signal30", "_end_"};
static const char* block9[7] = {"signal9", "signal24", "signal37", "signal1", "signal15", "signal26", "_end_"};
static const char* block10[10] = {"signal15", "signal26", "signal28", "signal30", "signal9", "signal24", "signal37", "signal39", "signal41", "_end_"};
static const char* block11[8] = {"signal9", "signal14", "signal24", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* block12to13[7] = {"signal9", "signal24", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* block14[8] = {"signal8", "signal23", "signal36", "signal15", "signal26", "signal28", "signal30", "_end_"};
static const char* block16to17[5] = {"signal15", "signal26", "signal28", "signal30", "_end_"};
static const char* block18[2] = {"signal20", "_end_"};
static const char* block19to22[6] = {"signal8", "signal23", "signal36", "signal4a", "signal18a", "_end_"};
static const char* p1[8] = {"signal4a", "signal5", "signal19", "signal45", "signal47", "signal49", "signal51", "_end_"};
static const char* p2[7] = {"signal4a", "signal5", "signal19", "signal45", "signal47", "signal49", "_end_"};
static const char* p3[7] = {"signal7a", "signal8", "signal22a", "signal46a", "signal48a", "signal50a", "_end_"};
static const char* p4[9] = {"signal7a", "signal8", "signal22a", "signal35a", "signal46a", "signal48a", "signal50a", "signal53a", "_end_"};
static const char* p5[8] = {"signal1", "signal9", "signal11", "signal15", "signal26", "signal28", "signal30", "_end_"};
static const char* p6to7[8] = {"signal1", "signal9", "signal14", "signal15", "signal24", "signal26", "signal37", "_end_"};
static const char* p8to9[8] = {"signal4a", "signal18a", "signal19", "signal45", "signal47", "signal49", "signal51", "_end_"};
static const char* p10[4] = {"signal20", "signal21", "signal43", "_end_"};
static const char* p11[8] = {"signal8", "signal22a", "signal23", "signal36", "signal46a", "signal48a", "signal50a", "_end_"};
static const char* p12[10] = {"signal8", "signal22a", "signal23", "signal35a", "signal36", "signal46a", "signal48a", "signal50a", "signal53a", "_end_"};
static const char* p13[9] = {"signal22a", "signal23", "signal35a", "signal36", "signal46a", "signal48a", "signal50a", "signal53a", "_end_"};
static const char* p14[8] = {"signal1", "signal15", "signal24", "signal26", "signal28", "signal30", "signal37", "_end_"};
static const char* p15to16[9] = {"signal1", "signal9", "signal15", "signal24", "signal26", "signal28", "signal30", "signal37", "_end_"};
static const char* p17[7] = {"signal1", "signal9", "signal15", "signal24", "signal26", "signal37", "_end_"};
static const char* p18a[9] = {"signal9", "signal14", "signal15", "signal24", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* p18b[10] = {"signal9", "signal14", "signal15", "signal24", "signal26", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* p19[9] = {"signal9", "signal15", "signal24", "signal26", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* p20to21[10] = {"signal9", "signal15", "signal24", "signal26", "signal28", "signal30", "signal37", "signal39", "signal41", "_end_"};
static const char* p22[9] = {"signal9", "signal24", "signal28", "signal30", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* p23[6] = {"signal15", "signal26", "signal28", "signal30", "signal32", "_end_"};
static const char* p24[6] = {"signal8", "signal23", "signal35a", "signal36", "signal53a", "_end_"};
static const char* p25[7] = {"signal15", "signal26", "signal28", "signal30", "signal39", "signal41", "_end_"};
static const char* p26[6] = {"signal4a", "signal18a", "signal45", "signal47", "signal49", "_end_"};
static const char* p27[7] = {"signal8", "signal23", "signal36", "signal46a", "signal48a", "signal50a", "_end_"};
static const char* p28[5] = {"signal4a", "signal18a", "signal47", "signal49", "_end_"};
static const char* p29[6] = {"signal8", "signal23", "signal36", "signal48a", "signal50a", "_end_"};
static const char* crossing1[11] = {"signal9", "signal15", "signal24", "signal26", "signal28", "signal30", "signal32", "signal37", "signal39", "signal41", "_end_"};
static const char* crossing2[7] = {"signal9", "signal14", "signal15", "signal24", "signal26", "signal37", "_end_"};

static const char** guarding_signals_mapping[44] = {block1, block2, block3, block4, block5, block6, block7, block8and15, block9, block10, block11, block12to13, block14, block16to17, block18, block19to22, p1, p2, p3, p4, p5, p6to7, p8to9, p10, p11, p12, p13, p14, p15to16, p17, p18a, p18b, p19, p20to21, p22, p23, p24, p25, p26, p27, p28, p29, crossing1, crossing2};

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
		selected_interlocker_name = NULL;
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

void release_all_interlockers(void) {
	if (selected_interlocker_name != NULL) {
		g_string_free(selected_interlocker_name, true);
		selected_interlocker_name = NULL;
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

size_t guarding_signals_lookup(const char* segment_id) {
if (strcmp(segment_id,"seg1") || strcmp(segment_id,"seg2") || strcmp(segment_id,"seg3")){ // block 1
		return 0;
	} else if (strcmp(segment_id,"seg6") || strcmp(segment_id,"seg7a") || strcmp(segment_id,"seg7b") || strcmp(segment_id,"seg8")){ // block 2
		return 1;
	} else if (strcmp(segment_id,"seg11") || strcmp(segment_id,"seg12") || strcmp(segment_id,"seg13")){ // block 3
		return  2;
	} else if (strcmp(segment_id,"seg15") || strcmp(segment_id,"seg16a") || strcmp(segment_id,"seg16b") || strcmp(segment_id,"seg17")){ // block 4
		return 3;
	} else if (strcmp(segment_id,"seg21b") || strcmp(segment_id,"seg22") || strcmp(segment_id,"seg23")){ // block 5
		return 4;
	} else if (strcmp(segment_id,"seg26") || strcmp(segment_id,"seg27") || strcmp(segment_id,"seg28")){ // block 6
		return 5;
	} else if (strcmp(segment_id,"seg30") || strcmp(segment_id,"seg31a") || strcmp(segment_id,"seg31b") || strcmp(segment_id,"seg32")){ // block 7 (platform4)
		return 6;
	} else if (strcmp(segment_id,"seg36") || strcmp(segment_id,"seg37") || strcmp(segment_id,"seg38")){ // block 8
		return 7;
	} else if (strcmp(segment_id,"seg42a") || strcmp(segment_id,"seg42b")){ // block 9
		return 8;
	} else if (strcmp(segment_id,"seg50")){ // block 10
		return 9;
	} else if (strcmp(segment_id,"seg47") || strcmp(segment_id,"seg46") || strcmp(segment_id,"seg45")){ // block 11 (platform3)
		return 10;
	} else if (strcmp(segment_id,"seg56") || strcmp(segment_id,"seg55") || strcmp(segment_id,"seg54") || strcmp(segment_id,"seg59") || strcmp(segment_id,"seg58") || strcmp(segment_id,"seg57")){ // block 12 (platform2) or block 13 (platform1)
		return 11;
	} else if (strcmp(segment_id,"seg65") || strcmp(segment_id,"seg64") || strcmp(segment_id,"seg63") || strcmp(segment_id,"seg62") || strcmp(segment_id,"seg61")){ // block 14 + seg65
		return 12;
	} else if (strcmp(segment_id,"seg67") || strcmp(segment_id,"seg68") || strcmp(segment_id,"seg69")){ // block 15
		return 7; // same as block 8 (re-check!)
	} else if (strcmp(segment_id,"seg71") || strcmp(segment_id,"seg72") || strcmp(segment_id,"seg73") || strcmp(segment_id,"seg74") || strcmp(segment_id,"seg75") || strcmp(segment_id,"seg76")){ // block 16 (platform7) or block 17 (platform6)
		return 13;
	} else if (strcmp(segment_id,"seg79") || strcmp(segment_id,"seg78b") || strcmp(segment_id,"seg78a") || strcmp(segment_id,"seg77")){ // block 18 (platform5)
		return 14;
	} else if (strcmp(segment_id,"seg81") || strcmp(segment_id,"seg82a") || strcmp(segment_id,"seg82b") || strcmp(segment_id,"seg83")
					|| strcmp(segment_id,"seg86") || strcmp(segment_id,"seg87a") || strcmp(segment_id,"seg87b") || strcmp(segment_id,"seg88")
					|| strcmp(segment_id,"seg90") || strcmp(segment_id,"seg91a") || strcmp(segment_id,"seg91b") || strcmp(segment_id,"seg92")
					|| strcmp(segment_id,"seg93") || strcmp(segment_id,"seg94a") || strcmp(segment_id,"seg94b") || strcmp(segment_id,"seg95")){ // block 19 to block 22
		return 15;
	} else if (strcmp(segment_id,"seg4")){ // p1
		return 16;
	} else if (strcmp(segment_id,"seg5")){ // p2
		return 17;
	} else if (strcmp(segment_id,"seg9")){ // p3
		return 18;
	} else if (strcmp(segment_id,"seg10")){ // p4
		return 19;
	} else if (strcmp(segment_id,"seg14")){ // p5
		return 20;
	} else if (strcmp(segment_id,"seg18") || strcmp(segment_id,"seg19")){ // p6 or p7
		return 21;
	} else if (strcmp(segment_id,"seg24") || strcmp(segment_id,"seg25")){ // p8 or p9
		return 22;
	} else if (strcmp(segment_id,"seg29")){ // p10
		return 23;
	} else if (strcmp(segment_id,"seg33")){ // p11
		return 24;
	} else if (strcmp(segment_id,"seg34")){ // p12
		return 25;
	} else if (strcmp(segment_id,"seg35")){ // p13
		return 26;
	} else if (strcmp(segment_id,"seg39")){ // p14
		return 27;
	} else if (strcmp(segment_id,"seg40") || strcmp(segment_id,"seg41")){ // p15 or p16
		return 28;
	} else if (strcmp(segment_id,"seg43")){ // p17
		return 29;
	} else if (strcmp(segment_id,"seg21a")){ // p18a
		return 30;
	} else if (strcmp(segment_id,"seg44")){ // p18b
		return 31;
	} else if (strcmp(segment_id,"seg48")){ // p19
		return 32;
	} else if (strcmp(segment_id,"seg49") || strcmp(segment_id,"seg51")){ // p20 or p21
		return 33;
	} else if (strcmp(segment_id,"seg53")){ // p22
		return 34;
	} else if (strcmp(segment_id,"seg60")){ // p23
		return 35;
	} else if (strcmp(segment_id,"seg66")){ // p24
		return 36;
	} else if (strcmp(segment_id,"seg70")){ // p25
		return 37;
	} else if (strcmp(segment_id,"seg80")){ // p26
		return 38;
	} else if (strcmp(segment_id,"seg84")){ // p27
		return 39;
	} else if (strcmp(segment_id,"seg85")){ // p28
		return 40;
	} else if (strcmp(segment_id,"seg89")){ // p29
		return 41;
	} else if (strcmp(segment_id,"seg52")){ // crossing1
		return 42;
	} else if (strcmp(segment_id,"seg20")){ // crossing2
		return 43;
	}
	/// TODO: Make platform independent - size_t is probably not a unsigned long everywhere.
	return ULONG_MAX; 
}

bool is_any_entry_signal_permissive(const char* segment_id) {
	size_t guarding_signals_lookup_index = guarding_signals_lookup(segment_id);
	if (guarding_signals_lookup_index >= ULONG_MAX) {
		return false;
	}
	const char** guarding_signals_elem = guarding_signals_mapping[guarding_signals_lookup_index];
	if (guarding_signals_elem == NULL) {
		return false;
	}
	for (size_t sig_i = 0; sig_i < 14; ++sig_i) {
		const char* guarding_signal_item = guarding_signals_elem[sig_i];
		if (strcmp(guarding_signal_item, "_end_")) {
			break;
		}
		if (guarding_signal_item != NULL) {
			t_bidib_unified_accessory_state_query signal_state = bidib_get_signal_state(guarding_signal_item);
			const char* guarding_signal_aspect_state = (signal_state.type == BIDIB_ACCESSORY_BOARD) ?
			                       signal_state.board_accessory_state.state_id :
			                       signal_state.dcc_accessory_state.state_id;
			/// TODO: Replace g_strrstr (substring search) with full string comparison 
			// (need to know precise aspect names for that)
			if (g_strrstr(guarding_signal_aspect_state, "go") || g_strrstr(guarding_signal_aspect_state, "shunt")) {
				bidib_free_unified_accessory_state_query(signal_state);
				return true;
			}
			bidib_free_unified_accessory_state_query(signal_state);
		}
	}
	return false;
}

bool is_any_segment_after_preceding_signal_until_segment_occupied(t_interlocking_route *route, size_t path_segment_index) {
	//First need to find the signal that precedes path_segment_index
	if (route == NULL || route->path == NULL || route->path->len == 0) {
		return false;
	}
	if (path_segment_index > route->path->len) {
		return false;
	}
	for (long long i = path_segment_index; i >= 0; --i) {
		const char* path_item = g_array_index(route->path, char*, (size_t) i);
		if (is_type_segment(path_item)) {
			// return true if segment is occupied,
			// otherwise continue searching.
			if (is_segment_occupied(path_item)) {
				return true;
			}
		} else if (is_type_signal(path_item)) {
			// Preceding signal has been found; thus end search.
			return false;
		}
	}
	return false;
}

bool is_route_conflict_safe_sectional(const char *granted_route_id, const char *requested_route_id) {
	t_interlocking_route *granted_route = get_route(granted_route_id);
	t_interlocking_route *requested_route = get_route(requested_route_id);
	
	if (granted_route == NULL || requested_route == NULL) {
		/// TODO: Log
		return false;
	}
	bool encountered_signal = true;
	// Search backwards in granted route to minimize duplicate lookups
	for (size_t gr_i = granted_route->path->len; gr_i > 0; --gr_i) {
		const char* gr_path_item = g_array_index(granted_route->path, char*, gr_i - 1);
		/// TODO: Remove the G_UNLIKELY if the routes contain intermediate interlocking signals
		if (G_UNLIKELY(is_type_signal(gr_path_item))) {
			// Further optimization: Only set encountered_signal to true if this is NOT a distant signal.
			encountered_signal = true;
			continue;
		}
		if (G_UNLIKELY(!is_type_segment(gr_path_item))) {
			continue;
		}
		for (size_t re_i = 0; re_i < requested_route->path->len; ++re_i) {
			const char* re_path_item = g_array_index(requested_route->path, char*, re_i);
			if (strcmp(gr_path_item, re_path_item)) {
				if (is_any_entry_signal_permissive(re_path_item)) {
					return false;
				}
				if (encountered_signal) {
					encountered_signal = false;
					if (is_any_segment_after_preceding_signal_until_segment_occupied(granted_route, gr_i)) {
						return false;
					}
				}
			}
		}
	}
	return true;
}

// get_granted_route_conflicts, but with direct implementation of sectional-style checker; 
// should thus be much more efficient
GArray *get_granted_route_conflicts_sectional(const char *route_id) {
	GArray* conflict_route_ids = g_array_new(FALSE, FALSE, sizeof(char *));

	char *conflict_routes[1024];
	const size_t conflict_routes_len = config_get_array_string_value("route", route_id, "conflicts", conflict_routes);
	for (size_t i = 0; i < conflict_routes_len; i++) {
		t_interlocking_route *conflict_route = get_route(conflict_routes[i]);
		if (conflict_route->train != NULL) {
			if (!is_route_conflict_safe_sectional(conflict_routes[i],route_id)) {
				const size_t conflict_route_id_string_len = strlen(conflict_route->id) + strlen(conflict_route->train) + 3 + 1;
				char *conflict_route_id_string = malloc(sizeof(char *) * conflict_route_id_string_len);
				snprintf(conflict_route_id_string, conflict_route_id_string_len, "%s (%s)",
							conflict_route->id, conflict_route->train);
				g_array_append_val(conflict_route_ids, conflict_route_id_string);
			}
		}
	}
	return conflict_route_ids;
}

GArray *get_granted_route_conflicts(const char *route_id) {
	GArray* conflict_route_ids = g_array_new(FALSE, FALSE, sizeof(char *));

	// When a sectional interlocker is in use, use the route_has_no_sectional_conflicts to
	// check for route availability.

	if (g_strrstr(selected_interlocker_name->str,"sectional") != NULL) {
		// When route has no sectional conflicts, directly return
		// with empty conflict_route_ids collection. Otherwise continue
		// with standard check.
		if (route_has_no_sectional_conflicts(route_id)) {
			return conflict_route_ids;
		}
	}

	char *conflict_routes[1024];
	const size_t conflict_routes_len = config_get_array_string_value("route", route_id, "conflicts", conflict_routes);
	for (size_t i = 0; i < conflict_routes_len; i++) {
		t_interlocking_route *conflict_route = get_route(conflict_routes[i]);
		if (conflict_route->train != NULL) {
			const size_t conflict_route_id_string_len = strlen(conflict_route->id) + strlen(conflict_route->train) + 3 + 1;
			char *conflict_route_id_string = malloc(sizeof(char *) * conflict_route_id_string_len);
			snprintf(conflict_route_id_string, conflict_route_id_string_len, "%s (%s)",
			         conflict_route->id, conflict_route->train);
			g_array_append_val(conflict_route_ids, conflict_route_id_string);
		}
	}

	return conflict_route_ids;
}

const bool get_route_is_clear(const char *route_id) {
	pthread_mutex_lock(&interlocker_mutex);

	bahn_data_util_init_cached_track_state();

	// Check that all route signals are in the Stop aspect
	char *signal_ids[1024];
	const size_t signal_ids_len = config_get_array_string_value("route", route_id, "route_signals", signal_ids);
	for (size_t i = 0; i < signal_ids_len; i++) {
		char *signal_state = track_state_get_value(signal_ids[i]);
		if (strcmp(signal_state, "stop")) {
			bahn_data_util_free_cached_track_state();
			pthread_mutex_unlock(&interlocker_mutex);
			return false;
		}
	}


	// Check that all blocks are unoccupied
	char *item_ids[1024];
	const size_t item_ids_len = config_get_array_string_value("route", route_id, "path", item_ids);
	for (size_t i = 0; i < item_ids_len; i++) {
		if (is_type_segment(item_ids[i]) && is_segment_occupied(item_ids[i])) {
			bahn_data_util_free_cached_track_state();
			pthread_mutex_unlock(&interlocker_mutex);
			return false;
		}
	}

	bahn_data_util_free_cached_track_state();

	pthread_mutex_unlock(&interlocker_mutex);
	return true;
}

bool route_has_no_sectional_conflicts(const char *route_id) {
	// 1. set inputs/context for check
	pthread_mutex_lock(&interlocker_mutex);
	bahn_data_util_init_cached_track_state();
	char checker_output[1024];
	char* route_id_copy = strdup(route_id);
	check_route_sectional_tick_data check_input_data = {route_id_copy, NULL, NULL, checker_output, -1};

	// 2. Reset execution context and set new input
	check_route_sectional_reset(&check_input_data);

	// 3. Do ticks until check has terminated
	do {
		check_route_sectional_tick(&check_input_data);
	} while (check_input_data.terminated != 1);

	// Iff route_id is returned, route is available (thus return true)
	bool ret = strcmp(check_input_data.out, route_id) == 0;
	pthread_mutex_unlock(&interlocker_mutex);
	free (route_id_copy);
	return ret;
}

GString *grant_route(const char *train_id, const char *source_id, const char *destination_id) {
	if (selected_interlocker_instance == -1) {
		syslog_server(LOG_ERR, "Grant route: No interlocker has been set");
		return g_string_new("no_interlocker");
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
	const char *route_id = interlocker_instance_io.output_route_id;
	if (route_id != NULL && params_check_is_number(route_id)) {
		syslog_server(LOG_NOTICE, "Grant route: Route %s has been granted", route_id);

		syslog_server(LOG_NOTICE, "Grant route: Set points and signals for route id \"%s\" - interlocker type %d",
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
	GString *route_id_copy = g_string_new(route_id);
	bahn_data_util_free_cached_track_state();

	pthread_mutex_unlock(&interlocker_mutex);
	return route_id_copy;
}

const char *grant_route_id(const char *train_id, const char *route_id) {
	// Check whether the route can be granted
	t_interlocking_route * const route = get_route(route_id);
	const GArray * const granted_conflicts = get_granted_route_conflicts(route_id);
	if (route->train != NULL || granted_conflicts->len > 0) {
		return "not_grantable";
	}

	// Check whether the route is physically available
	if (!get_route_is_clear(route_id)) {
		return "not_clear";
	}

	// Grant the route to the train and mark it unavailable
	route->train = strdup(train_id);

	// Set the points to their required positions
	for (size_t i = 0; i < route->points->len; i++) {
		const t_interlocking_point point =
				g_array_index(route->points, t_interlocking_point, i);
		const char *position = (point.position == NORMAL) ? "normal" : "reverse";
		bidib_switch_point(point.id, position);
		bidib_flush();
	}

	// Set the signals to their required aspects
	for (size_t i = 0; i < route->signals->len - 1; i++) {
		const char *signal = g_array_index(route->signals, char *, i);
		const char *signal_type = config_get_scalar_string_value("signal", signal, "type");
		const char *signal_aspect = strcmp(signal_type, "shunting") == 0 ? "aspect_shunt" : "aspect_go";
		bidib_set_signal(signal, signal_aspect);
		bidib_flush();
	}

	return "granted";
}

void release_route(const char *route_id) {
	pthread_mutex_lock(&interlocker_mutex);
	t_interlocking_route *route = get_route(route_id);
	if (route->train != NULL) {
		const char *signal_aspect = "aspect_stop";

		const int signal_count = route->signals->len;
		for (int signal_index = 0; signal_index < signal_count; signal_index++) {
			// Get each signal along the route
			const char *signal_id = g_array_index(route->signals, char *, signal_index);

			if (bidib_set_signal(signal_id, signal_aspect)) {
				syslog_server(LOG_ERR, "Release route: Unable to set signal to aspect %s", signal_aspect);
			}
		}
		bidib_flush();

		free(route->train);
		route->train = NULL;
		syslog_server(LOG_NOTICE, "Release route: route %s released", route_id);
    }

	pthread_mutex_unlock(&interlocker_mutex);
}

const bool reversers_state_update(void) {
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
			t_bidib_reverser_state_query rev_state_query =
					bidib_get_reverser_state(reverser_id);
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

onion_connection_status handler_release_route(void *_, onion_request *req,
                                          onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		if (strcmp(route_id, "") == 0) {
			syslog_server(LOG_ERR, "Request: Release route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			release_route(route_id);
			syslog_server(LOG_NOTICE, "Request: Release route - route: %s", route_id);
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
