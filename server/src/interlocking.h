/*
 *
 * Copyright (C) 2020 University of Bamberg, Software Technologies Research Group
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

#ifndef INTERLOCKING_H
#define INTERLOCKING_H

#include <glib.h>

typedef enum {
	CLOCKWISE,
	ANTICLOCKWISE
} e_interlocking_table;

typedef enum {
	NORMAL,
	REVERSE
} e_interlocking_point_position;

typedef struct {
	char *id;
	int bidib_state_index;	// t_bidib_track_state.segments[bidib_state_index].data
} t_interlocking_path_segment;

typedef struct {
	char *id;
	int bidib_state_index;	// t_bidib_track_state.points_board[bidib_state_index].data
	e_interlocking_point_position position;
} t_interlocking_point;

typedef struct {
	char *id;
	int bidib_state_index;	// t_bidib_track_state.signals_board[bidib_state_index].data
} t_interlocking_signal;

/**
 * Route information
 *
 * id of the route
 * source signal of the route
 * destination signal of the route
 * direction of the train movement on the route
 * path of the route, consisting of track segments
 * points within the route and their required position
 * signals within the route
 * conflicting routes
 * id of train when granted the route
 */
typedef struct {
	size_t id;
	t_interlocking_signal source;
	t_interlocking_signal destination;
	size_t direction;
	GArray *path;		// g_array_index(route->path, t_interlocking_path_segment, segment_index)
	GArray *points;		// g_array_index(route->points, t_interlocking_point, point_index)
	GArray *signals;	// g_array_index(route->signals, t_interlocking_signal, signal_index)
	GArray *conflicts;	// g_array_index(route->conflicts, size_t, conflict_index)
	GString *train_id;
} t_interlocking_route;

//extern t_interlocking_route interlocking_table[TOTAL_ROUTES];
extern GArray *interlocking_table;

typedef struct {
	char *string;
	int id;
} t_route_string_to_id;

extern GHashTable *route_string_to_ids_hashtable;


/**
 * Resolves the libbidib state array indices for track segments, 
 * points, and signals. Creates hashtable for constant lookup of
 * route strings to IDs.
 *
 * @return 0 if successful, otherwise 1
 */
int interlocking_table_initialise(const char *config_dir);

/**
 * Frees the hashtable that maps route strings to IDs.
 */
void free_interlocking_hashtable(void);

/**
 * Frees the array that stores the interlocking table.
 */
void free_interlocking_table(void);

/**
 * Returns the route ID for a given source and destination signal. 
 * The route ID is also the required array index for the interlocking table.
 *
 * @return the route ID if it exists, otherwise -1
 */
int interlocking_table_get_route_id(const char *source_id, const char *destination_id);

/**
 * Return the route (pointer to a struct) for a given route_id
 *
 * @param route_id route
 * @return the route pointer if it exists, otherwise NULL
 */
t_interlocking_route *get_route(int route_id);

#endif // INTERLOCKING_H

