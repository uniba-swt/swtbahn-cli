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
#include <string.h>
#include <stdio.h>

#include "interlocking.h"
#include "server.h"
#include "parsers/interlocking_parser.h"


GHashTable *route_hash_table = NULL;
GHashTable *route_string_to_ids_hashtable = NULL;

void free_interlocking_hashtable_key(void *pointer) {
    if (pointer != NULL) {
        free(pointer);
        pointer = NULL;
    }
}

void free_interlocking_hashtable_value(void *pointer) {
    // For use with route_string_to_ids_hashtable:
    // Not necessary to free elements individually, as they are not allocated/owned here,
    // they are owned by the route_hash_table instead.
    GArray *value = (GArray *)pointer;
    g_array_free(value, true);
}

// initialises the route_string_to_ids_hashtable hashtable.
static void create_route_str_to_ids_hashtable() {
    route_string_to_ids_hashtable = 
            g_hash_table_new_full(g_str_hash, g_str_equal, 
                                  free_interlocking_hashtable_key, 
                                  free_interlocking_hashtable_value);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, route_hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        t_interlocking_route *route = (t_interlocking_route *) value;

        // Build search key, example: signal3signal6
        size_t len = strlen(route->source) + strlen(route->destination) + 1;
        char *route_string = malloc(sizeof(char) * len);
        if (route_string == NULL) {
            syslog_server(LOG_ERR, 
                          "Interlocking create interlocking hash table: "
                          "unable to allocate memory for route_string");
            return;
        }
        snprintf(route_string, len, "%s%s", route->source, route->destination);

        if (g_hash_table_contains(route_string_to_ids_hashtable, route_string)) {
            // entry with route_string as key exists in hashtable
            void *route_ids_ptr = g_hash_table_lookup(route_string_to_ids_hashtable, route_string);
            free(route_string);
            // Add the route to the hashtable entry's value/list of routes
            GArray *route_ids = (GArray *) route_ids_ptr;
            g_array_append_val(route_ids, route->id);
        } else {
            // entry with route_string as key does NOT exist in hashtable -> create new
            GArray *route_ids = g_array_sized_new(FALSE, FALSE, sizeof(size_t), 8);
            g_array_append_val(route_ids, route->id);
            // Ownership of key (route_string) is transferred to hashtable
            g_hash_table_insert(route_string_to_ids_hashtable, route_string, route_ids);
        }
    }

    syslog_server(LOG_NOTICE, "Interlocking create hash table - done");
}

bool interlocking_table_initialise(const char *config_dir) {
    // Parse the interlocking table from the YAML file
    route_hash_table = parse_interlocking_table(config_dir);
    if (route_hash_table != NULL) {
        create_route_str_to_ids_hashtable();
        return true;
    }

    return false;
}

void free_interlocking_table(void) {
    // free route-string to route ids hash table
    if (route_string_to_ids_hashtable != NULL) {
        g_hash_table_destroy(route_string_to_ids_hashtable);
        route_string_to_ids_hashtable = NULL;
    }
    syslog_server(LOG_INFO, "Interlocking extra table route str to route ids freed");

    // free general route hash table
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
        char *route_id_string = strdup(route->id);
        if (route_id_string == NULL) {
            syslog_server(LOG_ERR, 
                          "Interlocking table get all route ids: "
                          "unable to allocate memory for a route_id_string");
            continue;
        }
        g_array_append_val(route_ids, route_id_string);
    }

    return route_ids;
}

GArray *interlocking_table_get_all_route_ids_cpy(void) {
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

const char *interlocking_table_get_route_id_of_train(const char *train_id) {
    if (train_id == NULL) {
        syslog_server(LOG_ERR, "Get route id of train: invalid train_id parameter (NULL)");
        return NULL;
    }
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, route_hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        const t_interlocking_route *route = (t_interlocking_route *) value;
        if (route != NULL && route->train != NULL && strcmp(route->train, train_id) == 0) {
            return route->id;
        }
    }
    return NULL;
}

GArray *interlocking_table_get_route_ids(const char *source_id, const char *destination_id) {
    if (source_id == NULL || destination_id == NULL) {
        syslog_server(LOG_ERR, 
                      "Get route ids for source-dest combination: invalid (NULL) parameters");
        return NULL;
    }
    const size_t len = strlen(source_id) + strlen(destination_id) + 1;
    char route_string[len];
    snprintf(route_string, len, "%s%s", source_id, destination_id);

    if (g_hash_table_contains(route_string_to_ids_hashtable, route_string)) {
        return (GArray *)g_hash_table_lookup(route_string_to_ids_hashtable, route_string);
    }

    return NULL;
}

t_interlocking_route *get_route(const char *route_id) {
    if (route_id == NULL) {
        return NULL;
    }
    if (g_hash_table_contains(route_hash_table, route_id)) {
        return g_hash_table_lookup(route_hash_table, route_id);
    }

    return NULL;
}

unsigned int interlocking_table_get_size() {
    if (route_hash_table != NULL) {
        return g_hash_table_size(route_hash_table);
    }

    return 0;
}
