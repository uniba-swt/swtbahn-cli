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
 * - Eugene Yip <https://github.com/eyip002>
 *
 */
 
#include <bidib.h>
#include <syslog.h>
#include <glib.h>
#include <stdio.h>

#include "interlocking.h"

t_interlocking_route interlocking_table_ultraloop[TOTAL_ROUTES] = {
	{
		.id               = 0,
		.source           = {.id = "signal3a", .bidib_state_index = -1},
		.destination      = {.id = "signal6", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1}
		                    },
		.path_count       = 3,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = REVERSE}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = "signal5", .bidib_state_index = -1}
		                    },
		.signals_count    = 1,
		.conflicts        = {1, 2, 3, 4, 5, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 1, 
		.source           = {.id = "signal3a", .bidib_state_index = -1},
		.destination      = {.id = "signal7", .bidib_state_index = -1},
		.path             = {
						        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg19", .bidib_state_index = -1}
							},
		.path_count       = 6,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = REVERSE}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
		                    },
		.signals_count    = 0,
		.conflicts        = {0, 2, 3, 4, 5, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 2, 
		.source           = {.id = "signal3b", .bidib_state_index = -1},
		.destination      = {.id = "signal6", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1}
		                    },
		.path_count       = 3,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = NORMAL}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = "signal5", .bidib_state_index = -1}
		                    },
		.signals_count    = 1,
		.conflicts        = {0, 1, 3, 4, 5, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 3, 
		.source           = {.id = "signal3b", .bidib_state_index = -1},
		.destination      = {.id = "signal7", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg19", .bidib_state_index = -1}
		                    },
		.path_count       = 6,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = NORMAL}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
							},
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 4, 5, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 4, 
		.source           = {.id = "signal5", .bidib_state_index = -1},
		.destination      = {.id = "signal4", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg31", .bidib_state_index = -1}
		                    },
		.path_count       = 5,
		.points           = {
		                        {.id = NULL, .bidib_state_index = -1, .position = -1}
		                    },
		.points_count     = 0,
		.signals          = {
		                        {.id = "signal3a", .bidib_state_index = -1}
		                    },
		.signals_count    = 1,
		.conflicts        = {0, 1, 2, 3, 5, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 5, 
		.source           = {.id = "signal5", .bidib_state_index = -1},
		.destination      = {.id = "signal1", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg32", .bidib_state_index = -1}
		                    },
		.path_count       = 6,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = REVERSE}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
		                    },
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 3, 4, 6, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 6, 
		.source           = {.id = "signal5", .bidib_state_index = -1},
		.destination      = {.id = "signal2", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg28", .bidib_state_index = -1}
		                    },
		.path_count       = 6,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = NORMAL}
							},
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
		                    },
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 3, 4, 5, 7, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 7, 
		.source           = {.id = "signal6", .bidib_state_index = -1},
		.destination      = {.id = "signal7", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg29", .bidib_state_index = -1},
		                        {.id = "seg17", .bidib_state_index = -1},
		                        {.id = "seg18", .bidib_state_index = -1},
		                        {.id = "seg19", .bidib_state_index = -1}
		                    },
		.path_count       = 5,
		.points           = {
		                        {.id = NULL, .bidib_state_index = -1, .position = -1}
		                    },
		.points_count     = 0,
		.signals          = {
							    {.id = NULL, .bidib_state_index = -1}
		                    },
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 3, 4, 5, 6, 8, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 8, 
		.source           = {.id = "signal4", .bidib_state_index = -1},
		.destination      = {.id = "signal1", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg32", .bidib_state_index = -1}
		                    },
		.path_count       = 3,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = REVERSE}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
		                    },
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 3, 4, 5, 6, 7, 9},
		.conflicts_count  = 9,
		.is_blocked       = false
	},
	{
		.id               = 9, 
		.source           = {.id = "signal4", .bidib_state_index = -1},
		.destination      = {.id = "signal2", .bidib_state_index = -1},
		.path             = {
		                        {.id = "seg30", .bidib_state_index = -1},
		                        {.id = "seg31", .bidib_state_index = -1},
		                        {.id = "seg28", .bidib_state_index = -1}
							},
		.path_count       = 3,
		.points           = {
		                        {.id = "point2", .bidib_state_index = -1, .position = NORMAL}
		                    },
		.points_count     = 1,
		.signals          = {
		                        {.id = NULL, .bidib_state_index = -1}
							},
		.signals_count    = 0,
		.conflicts        = {0, 1, 2, 3, 4, 5, 6, 7, 8},
		.conflicts_count  = 9,
		.is_blocked       = false
	}
};

t_route_string_to_id route_string_to_id_table[TOTAL_ROUTES] = {
	{.string = "signal3asignal6", .id = 0},
	{.string = "signal3asignal7", .id = 1},
	{.string = "signal3bsignal6", .id = 2},
	{.string = "signal3bsignal7", .id = 3},
	{.string = "signal5signal4",  .id = 4},
	{.string = "signal5signal1",  .id = 5},
	{.string = "signal5signal2",  .id = 6},
	{.string = "signal6signal7",  .id = 7},
	{.string = "signal4signal1",  .id = 8},
	{.string = "signal4signal2",  .id = 9}
};

GHashTable* route_string_to_id_hashtable;

int create_interlocking_hashtable(void) {
	route_string_to_id_hashtable = g_hash_table_new(g_str_hash, g_str_equal);
	for (size_t i = 0; i < TOTAL_ROUTES; i++) {
		g_hash_table_insert(route_string_to_id_hashtable, 
		                    route_string_to_id_table[i].string, 
		                    &route_string_to_id_table[i].id);
	}
	
	return 0;
}

void free_interlocking_hashtable(void) {
	g_hash_table_destroy(route_string_to_id_hashtable);
}


static int interlocking_table_resolve_indices(void) {
	for (size_t route_id = 0; route_id < TOTAL_ROUTES; route_id++) {
		// Resolve the libbidib state array indices for track segments, 
		// points, and signals.
	
		GString *log = g_string_new("Interlocking initialisation: ");
		g_string_append_printf(log, "Route id: %zu\n", interlocking_table_ultraloop[route_id].id);
		
		char *id = interlocking_table_ultraloop[route_id].source.id;
		interlocking_table_ultraloop[route_id].source.bidib_state_index = bidib_get_signal_state_index(id);
		if (interlocking_table_ultraloop[route_id].source.bidib_state_index == -1) {
			syslog(LOG_ERR, "Interlocking initialisation: %s not found in BiDiB state", id);
			return 1;
		}
		id = interlocking_table_ultraloop[route_id].destination.id;
		interlocking_table_ultraloop[route_id].destination.bidib_state_index = bidib_get_signal_state_index(id);
		if (interlocking_table_ultraloop[route_id].destination.bidib_state_index == -1) {
			syslog(LOG_ERR, "Interlocking initialisation: %s not found in BiDiB state", id);
			return 1;
		}
		
		g_string_append_printf(log, "source: %s (%d)\n", 
							   interlocking_table_ultraloop[route_id].source.id, 
		                       interlocking_table_ultraloop[route_id].source.bidib_state_index);
		g_string_append_printf(log, "destination: %s (%d)\n", 
		                       interlocking_table_ultraloop[route_id].destination.id, 
		                       interlocking_table_ultraloop[route_id].destination.bidib_state_index);
		
		size_t path_count = interlocking_table_ultraloop[route_id].path_count;
		g_string_append_printf(log, "Path: ");
		for (size_t segment_index = 0; segment_index < path_count; segment_index++) {
			id = interlocking_table_ultraloop[route_id].path[segment_index].id;
			interlocking_table_ultraloop[route_id].path[segment_index].bidib_state_index = bidib_get_segment_state_index(id);
		
			if (interlocking_table_ultraloop[route_id].path[segment_index].bidib_state_index == -1) {
				syslog(LOG_ERR, "Interlocking initialisation: %s not found in BiDiB state", id);
				return -1;
			}
		
			g_string_append_printf(log, "%s (%d), ", 
								   interlocking_table_ultraloop[route_id].path[segment_index].id,
 			                       interlocking_table_ultraloop[route_id].path[segment_index].bidib_state_index);
		}
		g_string_append_printf(log, "\n");
		
		size_t points_count = interlocking_table_ultraloop[route_id].points_count;
		g_string_append_printf(log, "Points: ");
		for (size_t point_index = 0; point_index < points_count; point_index++) {
			id = interlocking_table_ultraloop[route_id].points[point_index].id;
			interlocking_table_ultraloop[route_id].points[point_index].bidib_state_index = bidib_get_point_state_index(id);
			
			if (interlocking_table_ultraloop[route_id].points[point_index].bidib_state_index == -1) {
				syslog(LOG_ERR, "Interlocking initialisation: %s not found in BiDiB state", id);
				return -1;
			}
			
			g_string_append_printf(log, "%s (%d), ", 
			                       interlocking_table_ultraloop[route_id].points[point_index].id,
								   interlocking_table_ultraloop[route_id].points[point_index].bidib_state_index);
		}
		g_string_append_printf(log, "\n");
		
		size_t signals_count = interlocking_table_ultraloop[route_id].signals_count;
		g_string_append_printf(log, "Signals: ");
		for (size_t signal_index = 0; signal_index < signals_count; signal_index++) {
			id = interlocking_table_ultraloop[route_id].signals[signal_index].id;
			interlocking_table_ultraloop[route_id].signals[signal_index].bidib_state_index = bidib_get_signal_state_index(id);
			
			if (interlocking_table_ultraloop[route_id].signals[signal_index].bidib_state_index == -1) {
				syslog(LOG_ERR, "Interlocking initialisation: %s not found in BiDiB state", id);
				return -1;
			}
			
			g_string_append_printf(log, "%s (%d), ", 
			                       interlocking_table_ultraloop[route_id].signals[signal_index].id,
 			                       interlocking_table_ultraloop[route_id].signals[signal_index].bidib_state_index);
		}
		g_string_append_printf(log, "\n");
		
		size_t conflicts_count = interlocking_table_ultraloop[route_id].conflicts_count;
		g_string_append_printf(log, "Conflicts: ");
		for (size_t conflict_index = 0; conflict_index < conflicts_count; conflict_index++) {
			g_string_append_printf(log, "%zu, ", 
			                       interlocking_table_ultraloop[route_id].conflicts[conflict_index]);
		}
		
		syslog(LOG_NOTICE, "%s", log->str);
		g_string_free(log, true);
	}
	
	return 0;
}

int interlocking_table_initialise(void) {
	int err_indices = interlocking_table_resolve_indices();
	int err_hashtable = create_interlocking_hashtable();
	
	return (err_indices || err_hashtable);
}

int interlocking_table_get_route_id(const char *source_id, const char *destination_id) {
	GString *route_string = g_string_new(source_id);
	g_string_append(route_string, destination_id);
	
	void *route_id_ptr = g_hash_table_lookup(route_string_to_id_hashtable, route_string->str);
	if (route_id_ptr == NULL) {
		return -1;
	}

	const int route_id = *(int *)route_id_ptr;
	return route_id;
}
