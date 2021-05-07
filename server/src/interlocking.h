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
 * - Tri Nguyen <https://github.com/trinnguyen>
 *
 */

#ifndef INTERLOCKING_H
#define INTERLOCKING_H

#include <glib.h>
#include <stdbool.h>

typedef enum {
    NORMAL,
    REVERSE
} e_interlocking_point_position;

typedef struct {
    char *id;
    e_interlocking_point_position position;
} t_interlocking_point;

/**
 * Route information
 *
 * id of the route
 * source signal of the route
 * destination signal of the route
 * path of the route, consisting of track segments
 * points within the route and their required position
 * signals within the route
 * conflicting routes
 * id of train when granted the route
 */
typedef struct {
    char *id;
    char *source;
    char *destination;
    float length;
    GArray *path;		// g_array_index(route->path, char *, segment_index)
    GArray *sections;		// g_array_index(route->sections, char *, segment_index)
    GArray *points;		// g_array_index(route->points, t_interlocking_point, point_index)
    GArray *signals;	// g_array_index(route->signals, char *, signal_index)
    GArray *conflicts;	// g_array_index(route->conflicts, char *, conflict_index)
    char *train;
} t_interlocking_route;


/**
 * Resolves the libbidib state array indices for track segments,
 * points, and signals. Creates hashtable for constant lookup of
 * route strings to IDs.
 *
 * @return true if successful, otherwise false
 */
bool interlocking_table_initialise(const char *config_dir);

/**
 * Free the array that stores interlocking table
 */
void free_interlocking_table(void);

/**
 * Return the array of route ID for a given source and destination signal
 * @return array if it exists, otherwise NULL
 */
GArray *interlocking_table_get_route_ids(const char *source_id, const char *destination_id);

/**
 * Return first route
 *
 * @param source_id
 * @param destination_id
 * @return
 */
int interlocking_table_get_route_id(const char *source_id, const char *destination_id);

/**
 * Return the route (pointer to a struct) for a given route_id
 * This method should be removed soon, the route_id is now in type 'char *', although the value is still a number
 *
 * @param route_id route
 * @return the route pointer if it exists, otherwise NULL
 */
t_interlocking_route *get_route(int route_id);

/**
 * Return the route (pointer to a struct) for a given route_id
 *
 * @param route_id route
 * @return the route pointer if it exists, otherwise NULL
 */
t_interlocking_route *get_route_str(char *route_id);

#endif // INTERLOCKING_H

