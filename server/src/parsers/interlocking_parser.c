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
 *
 */

#include "interlocking_parser.h"
#include "../interlocking.h"
#import "../server.h"

#include <stdio.h>
#include <yaml.h>

GHashTable *parse(yaml_parser_t *parser);

bool is_str_equal(const char *str1, const char *str2) {
    return strcmp(str1, str2) == 0;
}

bool init_parser(const char *config_dir, const char *table_file,
                 FILE **fh, yaml_parser_t *parser) {
    // generate full path
    size_t len = strlen(config_dir) + strlen(table_file) + 1;
    char full_path[len];
    snprintf(full_path, len, "%s%s", config_dir, table_file);

    // init file
    *fh = fopen(full_path, "r");

    if (*fh == NULL) {
        syslog_server(LOG_ERR, "%Failed to open %s", table_file);
        return false;
    }

    if (!yaml_parser_initialize(parser)) {
        fclose(*fh);
        syslog_server(LOG_ERR, "Failed to initialize interlocking table parser");
        return false;
    }

    yaml_parser_set_input_file(parser, *fh);
    return true;
}

typedef enum {
    SEQUENCE_NONE,
    SEQUENCE_ROUTES,
    SEQUENCE_PATH,
    SEQUENCE_SECTIONS,
    SEQUENCE_POINTS,
    SEQUENCE_SIGNALS,
    SEQUENCE_CONFLICTS
} e_sequence_level;

typedef enum {
    MAPPING_TABLE,
    MAPPING_ROUTE,
    MAPPING_SEGMENT,
    MAPPING_BLOCK,
    MAPPING_POINT,
    MAPPING_SIGNAL,
    MAPPING_CONFLICT
} e_mapping_level;

e_mapping_level decrease_mapping_level (e_mapping_level level) {
    switch (level) {
        case MAPPING_ROUTE:
            return MAPPING_TABLE;
        case MAPPING_SEGMENT:
        case MAPPING_BLOCK:
        case MAPPING_POINT:
        case MAPPING_SIGNAL:
        case MAPPING_CONFLICT:
            return MAPPING_ROUTE;
        default:
            return level;
    }
}

e_sequence_level decrease_sequence_level (e_sequence_level level) {
    switch (level) {
        case SEQUENCE_ROUTES:
            return SEQUENCE_NONE;
        case SEQUENCE_PATH:
        case SEQUENCE_SECTIONS:
        case SEQUENCE_POINTS:
        case SEQUENCE_SIGNALS:
        case SEQUENCE_CONFLICTS:
            return SEQUENCE_ROUTES;
        default:
            return level;
    }
}

void free_route_key(void *pointer) {
    free(pointer);
}

void free_route(void *item) {
    t_interlocking_route *route = (t_interlocking_route *) item;
    free(route->id);
    free(route->source);
    free(route->destination);
    if (route->train != NULL) {
        free(route->train);
    }

    if (route->path != NULL) {
        g_array_free(route->path, true);
    }

    if (route->sections != NULL) {
        g_array_free(route->sections, true);
    }

    if (route->points != NULL) {
        g_array_free(route->points, true);
    }

    if (route->signals != NULL) {
        g_array_free(route->signals, true);
    }

    if (route->conflicts != NULL) {
        g_array_free(route->conflicts, true);
    }
}

void free_interlocking_point(void *item) {
    t_interlocking_point *point = (t_interlocking_point *) item;
    free(point->id);
}

GHashTable *parse(yaml_parser_t *parser) {
    e_mapping_level cur_mapping = MAPPING_TABLE;
    e_sequence_level cur_sequence = SEQUENCE_NONE;

    GHashTable *routes = NULL;
    t_interlocking_route *route = NULL;
    t_interlocking_point *point = NULL;

    yaml_event_t  event;
    bool error = false;
    char *cur_scalar = NULL;
    char *last_scalar = NULL;

    do {
        if (!yaml_parser_parse(parser, &event)) {
            syslog_server(LOG_ERR, "Parser error %d\n", (*parser).error);
            break;
        }

        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        switch(event.type) {
            case YAML_NO_EVENT:
                error = true;
                break;

                /* Stream start/end */
            case YAML_STREAM_START_EVENT:
                break;
            case YAML_STREAM_END_EVENT:
                break;

                /* Block delimeters */
            case YAML_DOCUMENT_START_EVENT:
                break;
            case YAML_DOCUMENT_END_EVENT:
                break;

                /* Array */
            case YAML_SEQUENCE_START_EVENT:
                // routes
                if (cur_mapping == MAPPING_TABLE) {
                    if (is_str_equal(last_scalar, "interlocking-table")) {
                        cur_sequence = SEQUENCE_ROUTES;
                        if (routes == NULL) {
                            routes = g_hash_table_new_full(g_str_hash, g_str_equal, free_route_key, free_route);
                        }
                    }
                    break;
                }

                // array member of route
                if (cur_mapping == MAPPING_ROUTE) {
                    if (is_str_equal(last_scalar, "path")) {
                        route->path = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_PATH;
                        break;
                    }

                    if (is_str_equal(last_scalar, "sections")) {
                        route->sections = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_SECTIONS;
                        break;
                    }

                    if (is_str_equal(last_scalar, "points")) {
                        route->points = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_point), 8);
                        g_array_set_clear_func(route->points, free_interlocking_point);
                        cur_sequence = SEQUENCE_POINTS;
                        break;
                    }

                    if (is_str_equal(last_scalar, "signals")) {
                        route->signals = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_SIGNALS;
                        break;
                    }

                    if (is_str_equal(last_scalar, "conflicts")) {
                        route->conflicts = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_CONFLICTS;
                    }
                }
                break;
            case YAML_SEQUENCE_END_EVENT:
                cur_sequence = decrease_sequence_level(cur_sequence);
                break;

                /* Object */
            case YAML_MAPPING_START_EVENT:
                // routes -> create route
                if (cur_sequence == SEQUENCE_ROUTES) {
                    cur_mapping = MAPPING_ROUTE;
                    route = malloc(sizeof(t_interlocking_route));
                    route->id = NULL;
                    route->source = NULL;
                    route->destination = NULL;
                    route->length = 0;
                    route->path = NULL;
                    route->sections = NULL;
                    route->points = NULL;
                    route->signals = NULL;
                    route->conflicts = NULL;
                    route->train = NULL;
                    break;
                }

                // path -> create segment
                if (cur_sequence == SEQUENCE_PATH) {
                    cur_mapping = MAPPING_SEGMENT;
                    break;
                }

                if (cur_sequence == SEQUENCE_SECTIONS) {
                    cur_mapping = MAPPING_BLOCK;
                    break;
                }

                if (cur_sequence == SEQUENCE_POINTS) {
                    cur_mapping = MAPPING_POINT;
                    g_array_append_val(route->points, (t_interlocking_point){});
                    point = &g_array_index(route->points, t_interlocking_point, route->points->len - 1);
                    break;
                }

                if (cur_sequence == SEQUENCE_SIGNALS) {
                    cur_mapping = MAPPING_SIGNAL;
                    break;
                }

                if (cur_sequence == SEQUENCE_CONFLICTS) {
                    cur_mapping = MAPPING_CONFLICT;
                    break;
                }
                break;
            case YAML_MAPPING_END_EVENT:
                // insert route
                if (cur_mapping == MAPPING_ROUTE && route != NULL && route->id != NULL) {
                    g_hash_table_insert(routes, strdup(route->id), route);
                }

                // move up one level
                cur_mapping = decrease_mapping_level(cur_mapping);
                break;

                /* Data */
            case YAML_ALIAS_EVENT:
                error = true;
                break;
            case YAML_SCALAR_EVENT:
                cur_scalar = (char*)event.data.scalar.value;

                if (last_scalar == NULL) {
                    break;
                }

                // route
                if (cur_mapping == MAPPING_ROUTE) {
                    if (is_str_equal(last_scalar, "id")) {
                        route->id = cur_scalar;
                        break;
                    }

                    if (is_str_equal(last_scalar, "source")) {
                        route->source = cur_scalar;
                        break;
                    }

                    if (is_str_equal(last_scalar, "destination")) {
                        route->destination = cur_scalar;
                        break;
                    }

                    if (is_str_equal(last_scalar, "length")) {
                        route->length = strtof(cur_scalar, NULL);
                        break;
                    }

                    break;
                }

                // segment
                if (cur_mapping == MAPPING_SEGMENT) {
                    if (is_str_equal(last_scalar, "id")) {
                        g_array_append_val(route->path, cur_scalar);
                    }
                    break;
                }

                // block
                if (cur_mapping == MAPPING_BLOCK) {
                    if (is_str_equal(last_scalar, "id")) {
                        g_array_append_val(route->sections, cur_scalar);
                    }
                    break;
                }

                // point
                if (cur_mapping == MAPPING_POINT) {
                    if (is_str_equal(last_scalar, "id")) {
                        point->id = cur_scalar;
                        break;
                    }

                    if (is_str_equal(last_scalar, "position")) {
                        point->position = is_str_equal(cur_scalar, "reverse")
                                          ? REVERSE
                                          : NORMAL;
                    }

                    break;
                }

                // signal
                if (cur_mapping == MAPPING_SIGNAL) {
                    if (is_str_equal(last_scalar, "id")) {
                        g_array_append_val(route->signals, cur_scalar);
                    }

                    break;
                }

                // conflict
                if (cur_mapping == MAPPING_CONFLICT) {
                    if (is_str_equal(last_scalar, "id")) {
                        g_array_append_val(route->conflicts, cur_scalar);
                    }

                    break;
                }

                break;
            default:
                error = true;
                break;
        }

        last_scalar = cur_scalar;
    } while(!error);

    return routes;
}

GHashTable *parse_interlocking_table(const char *config_dir) {
    if (config_dir == NULL) {
        syslog_server(LOG_INFO, "No interlocking table loaded because of missing config dir");
        return false;
    }

    // init
    FILE *fh;
    yaml_parser_t parser;
    if (!init_parser(config_dir, "interlocking_table.yml", &fh, &parser)) {
        return false;
    }

    // parse
    GHashTable *routes = parse(&parser);

    // clean
    yaml_parser_delete(&parser);
    fclose(fh);

    // success
    if (routes != NULL) {
        syslog_server(LOG_INFO, "Interlocking table loaded successfully: %d", g_hash_table_size(routes));
        return routes;
    }

    // error
    syslog_server(LOG_ERR, "Failed to load interlocking table");
    return NULL;
}
