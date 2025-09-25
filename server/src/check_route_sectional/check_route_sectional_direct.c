/*
 *
 * Copyright (C) 2022 University of Bamberg, Software Technologies Research Group
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
 * - Bernhard Luedtke <https://github.com/BLuedtke>
 *
 */
#include "check_route_sectional_direct.h"
#include "../interlocking.h"
#include "../bahn_data_util.h"

#include <bidib/bidib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

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

// Don't forget to update this when changing the entry signal arrays above!
// -> If we ever get constexpr or consteval, shall compute this value at compile-time
static const unsigned int longest_entry_signals_array_len = 14;

static const char** entry_signals_mapping[44] = {block1, block2, block3, block4, block5, block6, block7, block8and15, block9, block10, block11, block12to13, block14, block16to17, block18, block19to22, p1, p2, p3, p4, p5, p6to7, p8to9, p10, p11, p12, p13, p14, p15to16, p17, p18a, p18b, p19, p20to21, p22, p23, p24, p25, p26, p27, p28, p29, crossing1, crossing2};

// Returns unsigned int between 0 and 43 (both inclusive) if segment_id is valid; returns
// 65535 if segment_id unknown or NULL
unsigned int entry_signals_lookup(const char* segment_id);

// If any signal controlling entry into the railway network section in which segment_id lies
// is in a permissive aspect (GO, SHUNT), return true.
// If segment_id is NULL, returns false.
bool is_any_entry_signal_permissive(const char* segment_id);

// Checks if any segment from route->path between the path item at path_segment_index
// and the last path item of type signal whose index is lower than path_segment_index is occupied.
// If route has no path elements, or if path_segment_index is larger than the length of the
// route, returns false.
bool is_any_segment_after_preceding_signal_until_segment_occupied(const t_interlocking_route *route, unsigned int path_segment_index);



bool is_route_conflict_safe_sectional(const char *granted_route_id, const char *requested_route_id) {
	t_interlocking_route *granted_route = get_route(granted_route_id);
	t_interlocking_route *requested_route = get_route(requested_route_id);
	
	if (granted_route == NULL || requested_route == NULL) {
		/// TODO: Log
		return false;
	}
	bool encountered_signal = true;
	// Search backwards in granted route to minimize duplicate lookups
	for (unsigned int gr_i = granted_route->path->len; gr_i > 0; --gr_i) {
		const char* gr_path_item = g_array_index(granted_route->path, char*, gr_i - 1);
		if (gr_path_item == NULL) {
			continue;
		}
		if (is_type_signal(gr_path_item)) {
			// Further optimization: Only set encountered_signal to true if this is NOT a distant signal.
			// -> but distant signals are currently (Jan. 2025) not contained in route def., so doesnt matter.
			encountered_signal = true;
			continue;
		}
		if (!is_type_segment(gr_path_item)) {
			continue;
		}
		for (unsigned int re_i = 0; re_i < requested_route->path->len; ++re_i) {
			const char* re_path_item = g_array_index(requested_route->path, char*, re_i);
			if (re_path_item == NULL) {
				continue;
			}
			if (strcmp(gr_path_item, re_path_item) == 0) {
				if (is_any_entry_signal_permissive(re_path_item)) {
					return false;
				}
				if (encountered_signal) {
					encountered_signal = false;
					if (is_any_segment_after_preceding_signal_until_segment_occupied(granted_route, gr_i - 1)) {
						return false;
					}
				}
			}
		}
	}
	return true;
}


bool is_any_segment_after_preceding_signal_until_segment_occupied(const t_interlocking_route *route, unsigned int path_segment_index) {
	if (route == NULL || route->path == NULL || route->path->len == 0) {
		return false;
	}
	if (path_segment_index > route->path->len) {
		return false;
	}
	// Loop from index to start checking at, decrementing until first signal is encountered.
	for (long i = (long) path_segment_index; i >= 0; --i) {
		const char* path_item = g_array_index(route->path, char*, i);
		if (is_type_segment(path_item)) {
			// return true if segment is occupied, otherwise continue searching.
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

bool is_any_entry_signal_permissive(const char* segment_id) {
	if (segment_id == NULL) {
		return false;
	}
	// Query for the entry signals relevant for segment_id
	unsigned int entry_signals_lookup_index = entry_signals_lookup(segment_id);
	if (entry_signals_lookup_index >= 65535) {
		return false;
	}
	const char** entry_signals_for_segment = entry_signals_mapping[entry_signals_lookup_index];
	if (entry_signals_for_segment == NULL) {
		return false;
	}
	// Check state of each entry signal
	for (unsigned int sig_i = 0; sig_i < longest_entry_signals_array_len; ++sig_i) {
		const char* entry_signal_item = entry_signals_for_segment[sig_i];
		if (strcmp(entry_signal_item, "_end_") == 0) {
			// End of entry_signals_for_segment
			break;
		}
		if (entry_signal_item != NULL) {
			t_bidib_unified_accessory_state_query signal_state = bidib_get_signal_state(entry_signal_item);
			const char* entry_signal_aspect_state = (signal_state.type == BIDIB_ACCESSORY_BOARD) ?
			                                            signal_state.board_accessory_state.state_id :
			                                            signal_state.dcc_accessory_state.state_id;
			// Could replace g_strrstr (substring search) with direct string comparison 
			// (need to know precise aspect names for that) -> look them up. Determining them dynamically
			// is possible but costs time
			if (g_strrstr(entry_signal_aspect_state, "go") != NULL || g_strrstr(entry_signal_aspect_state, "shunt") != NULL) {
				bidib_free_unified_accessory_state_query(signal_state);
				return true;
			}
			bidib_free_unified_accessory_state_query(signal_state);
		}
	}
	return false;
}


unsigned int entry_signals_lookup(const char* segment_id) {
	if (segment_id == NULL) {
		return 65535;
	} else if (strcmp(segment_id,"seg1") == 0 || strcmp(segment_id,"seg2") == 0 || strcmp(segment_id,"seg3") == 0) { // block 1
		return 0;
	} else if (strcmp(segment_id,"seg6") == 0 || strcmp(segment_id,"seg7a") == 0 || strcmp(segment_id,"seg7b") == 0 || strcmp(segment_id,"seg8") == 0) { // block 2
		return 1;
	} else if (strcmp(segment_id,"seg11") == 0 || strcmp(segment_id,"seg12") == 0 || strcmp(segment_id,"seg13") == 0) { // block 3
		return  2;
	} else if (strcmp(segment_id,"seg15") == 0 || strcmp(segment_id,"seg16a") == 0 || strcmp(segment_id,"seg16b") == 0 || strcmp(segment_id,"seg17") == 0) { // block 4
		return 3;
	} else if (strcmp(segment_id,"seg21b") == 0 || strcmp(segment_id,"seg22") == 0 || strcmp(segment_id,"seg23") == 0) { // block 5
		return 4;
	} else if (strcmp(segment_id,"seg26") == 0 || strcmp(segment_id,"seg27") == 0 || strcmp(segment_id,"seg28") == 0) { // block 6
		return 5;
	} else if (strcmp(segment_id,"seg30") == 0 || strcmp(segment_id,"seg31a") == 0 || strcmp(segment_id,"seg31b") == 0 || strcmp(segment_id,"seg32") == 0) { // block 7 (platform4)
		return 6;
	} else if (strcmp(segment_id,"seg36") == 0 || strcmp(segment_id,"seg37") == 0 || strcmp(segment_id,"seg38") == 0) { // block 8
		return 7;
	} else if (strcmp(segment_id,"seg42a") == 0 || strcmp(segment_id,"seg42b") == 0) { // block 9
		return 8;
	} else if (strcmp(segment_id,"seg50") == 0) { // block 10
		return 9;
	} else if (strcmp(segment_id,"seg47") == 0 || strcmp(segment_id,"seg46") == 0 || strcmp(segment_id,"seg45") == 0) { // block 11 (platform3)
		return 10;
	} else if (strcmp(segment_id,"seg56") == 0 || strcmp(segment_id,"seg55") == 0 || strcmp(segment_id,"seg54") == 0 || strcmp(segment_id,"seg59") == 0 || strcmp(segment_id,"seg58") == 0 || strcmp(segment_id,"seg57") == 0) { // block 12 (platform2) or block 13 (platform1)
		return 11;
	} else if (strcmp(segment_id,"seg65") == 0 || strcmp(segment_id,"seg64") == 0 || strcmp(segment_id,"seg63") == 0 || strcmp(segment_id,"seg62") == 0 || strcmp(segment_id,"seg61") == 0) { // block 14 + seg65
		return 12;
	} else if (strcmp(segment_id,"seg67") == 0 || strcmp(segment_id,"seg68") == 0 || strcmp(segment_id,"seg69") == 0) { // block 15
		return 7; // same as block 8 (re-check!)
	} else if (strcmp(segment_id,"seg71") == 0 || strcmp(segment_id,"seg72") == 0 || strcmp(segment_id,"seg73") == 0 || strcmp(segment_id,"seg74") == 0 || strcmp(segment_id,"seg75") == 0 || strcmp(segment_id,"seg76") == 0) { // block 16 (platform7) or block 17 (platform6)
		return 13;
	} else if (strcmp(segment_id,"seg79") == 0 || strcmp(segment_id,"seg78b") == 0 || strcmp(segment_id,"seg78a") == 0 || strcmp(segment_id,"seg77") == 0) { // block 18 (platform5)
		return 14;
	} else if (strcmp(segment_id,"seg81") == 0 || strcmp(segment_id,"seg82a") == 0 || strcmp(segment_id,"seg82b") == 0 || strcmp(segment_id,"seg83") == 0
					|| strcmp(segment_id,"seg86") == 0 || strcmp(segment_id,"seg87a") == 0 || strcmp(segment_id,"seg87b") == 0 || strcmp(segment_id,"seg88") == 0
					|| strcmp(segment_id,"seg90") == 0 || strcmp(segment_id,"seg91a") == 0 || strcmp(segment_id,"seg91b") == 0 || strcmp(segment_id,"seg92") == 0
					|| strcmp(segment_id,"seg93") == 0 || strcmp(segment_id,"seg94a") == 0 || strcmp(segment_id,"seg94b") == 0 || strcmp(segment_id,"seg95") == 0) { // block 19 to block 22
		return 15;
	} else if (strcmp(segment_id,"seg4") == 0) { // p1
		return 16;
	} else if (strcmp(segment_id,"seg5") == 0) { // p2
		return 17;
	} else if (strcmp(segment_id,"seg9") == 0) { // p3
		return 18;
	} else if (strcmp(segment_id,"seg10") == 0) { // p4
		return 19;
	} else if (strcmp(segment_id,"seg14") == 0) { // p5
		return 20;
	} else if (strcmp(segment_id,"seg18") == 0 || strcmp(segment_id,"seg19") == 0) { // p6 or p7
		return 21;
	} else if (strcmp(segment_id,"seg24") == 0 || strcmp(segment_id,"seg25") == 0) { // p8 or p9
		return 22;
	} else if (strcmp(segment_id,"seg29") == 0) { // p10
		return 23;
	} else if (strcmp(segment_id,"seg33") == 0) { // p11
		return 24;
	} else if (strcmp(segment_id,"seg34") == 0) { // p12
		return 25;
	} else if (strcmp(segment_id,"seg35") == 0) { // p13
		return 26;
	} else if (strcmp(segment_id,"seg39") == 0) { // p14
		return 27;
	} else if (strcmp(segment_id,"seg40") == 0 || strcmp(segment_id,"seg41") == 0) { // p15 or p16
		return 28;
	} else if (strcmp(segment_id,"seg43") == 0) { // p17
		return 29;
	} else if (strcmp(segment_id,"seg21a") == 0) { // p18a
		return 30;
	} else if (strcmp(segment_id,"seg44") == 0) { // p18b
		return 31;
	} else if (strcmp(segment_id,"seg48") == 0) { // p19
		return 32;
	} else if (strcmp(segment_id,"seg49") == 0 || strcmp(segment_id,"seg51") == 0) { // p20 or p21
		return 33;
	} else if (strcmp(segment_id,"seg53") == 0) { // p22
		return 34;
	} else if (strcmp(segment_id,"seg60") == 0) { // p23
		return 35;
	} else if (strcmp(segment_id,"seg66") == 0) { // p24
		return 36;
	} else if (strcmp(segment_id,"seg70") == 0) { // p25
		return 37;
	} else if (strcmp(segment_id,"seg80") == 0) { // p26
		return 38;
	} else if (strcmp(segment_id,"seg84") == 0) { // p27
		return 39;
	} else if (strcmp(segment_id,"seg85") == 0) { // p28
		return 40;
	} else if (strcmp(segment_id,"seg89") == 0) { // p29
		return 41;
	} else if (strcmp(segment_id,"seg52") == 0) { // crossing1
		return 42;
	} else if (strcmp(segment_id,"seg20") == 0) { // crossing2
		return 43;
	}
	return 65535; 
}
