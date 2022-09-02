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
 * - Tri Nguyen <https://github.com/trinnguyen>
 * - Eugene Yip <https://github.com/eyip002>
 *
 */

#include <bidib/bidib.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

#include "interlocking.h"
#include "server.h"
#include "parsers/interlocking_parser.h"


GHashTable *route_hash_table = NULL;
GHashTable *route_string_to_ids_hashtable = NULL;

void free_interlocking_hashtable_key(void *pointer) {
    char *key = (char *)pointer;
    free(key);
}

void free_interlocking_hashtable_value(void *pointer) {
    GArray *value = (GArray *)pointer;
    g_array_free(value, true);
}

void create_interlocking_hashtable(void) {
    route_string_to_ids_hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, free_interlocking_hashtable_key, free_interlocking_hashtable_value);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, route_hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        t_interlocking_route *route = (t_interlocking_route *) value;

        // Build key, example: signal3signal6
        size_t len = strlen(route->source) + strlen(route->destination) + 1;
        char *route_string = malloc(sizeof(char*) * len);
        snprintf(route_string, len, "%s%s", route->source, route->destination);

        if (g_hash_table_contains(route_string_to_ids_hashtable, route_string)) {
            void *route_ids_ptr = g_hash_table_lookup(route_string_to_ids_hashtable, route_string);
            GArray *route_ids = (GArray *) route_ids_ptr;
            g_array_append_val(route_ids, route->id);
        } else {
            GArray *route_ids = g_array_sized_new(FALSE, FALSE, sizeof(size_t), 8);
            g_array_append_val(route_ids, route->id);
            g_hash_table_insert(route_string_to_ids_hashtable, route_string, route_ids);
        }
    }

    syslog_server(LOG_NOTICE, "Interlocking create hash table: successful");
}

bool interlocking_table_initialise(const char *config_dir) {
    // Parse the interlocking table from the YAML file
    route_hash_table = parse_interlocking_table(config_dir);
    if (route_hash_table != NULL) {
        create_interlocking_hashtable();
        return true;
    }

    return false;
}

void free_interlocking_table(void) {
    // free hash table
    if (route_string_to_ids_hashtable != NULL) {
        g_hash_table_destroy(route_string_to_ids_hashtable);
        route_string_to_ids_hashtable = NULL;
    }

    // free array
    if (route_hash_table != NULL) {
        g_hash_table_destroy(route_hash_table);
        route_hash_table = NULL;
    }

    syslog_server(LOG_NOTICE, "Interlocking table freed");
}

GArray *interlocking_table_get_all_route_ids(void) {
	GArray* route_ids = g_array_new(FALSE, FALSE, sizeof(char *));
	
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, route_hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        t_interlocking_route *route = (t_interlocking_route *) value;
        g_array_append_val(route_ids, route->id);
	}
	
	return route_ids;
}

GArray *interlocking_table_get_route_ids(const char *source_id, const char *destination_id) {
    size_t len = strlen(source_id) + strlen(destination_id) + 1;
    char route_string[len];
    snprintf(route_string, len, "%s%s", source_id, destination_id);

    if (g_hash_table_contains(route_string_to_ids_hashtable, route_string)) {
        return (GArray *)g_hash_table_lookup(route_string_to_ids_hashtable, route_string);
    }

    return NULL;
}

int interlocking_table_get_route_id(const char *source_id, const char *destination_id) {
    GArray *route_ids = interlocking_table_get_route_ids(source_id, destination_id);

    // Return first route
    if (route_ids != NULL && route_ids->len > 0) {
        char *id = g_array_index(route_ids, char *, 0);
        return atoi(id);
    }

    return -1;
}

t_interlocking_route *get_route(const char *route_id) {
    if (g_hash_table_contains(route_hash_table, route_id)) {
        return g_hash_table_lookup(route_hash_table, route_id);
    }

    return NULL;
}
