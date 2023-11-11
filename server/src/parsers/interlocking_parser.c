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
#include "../server.h"
#include "parser_util.h"

#include <stdio.h>
#include <yaml.h>

const char INTERLOCKING_TABLE_FILENAME[] = "interlocking_table.yml";

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
        syslog_server(LOG_ERR, "%Interlocking parser: Failed to open %s", full_path);
        return false;
    }

    if (!yaml_parser_initialize(parser)) {
        fclose(*fh);
        syslog_server(LOG_ERR, "Interlocking parser: Failed to initialise the interlocking table parser");
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

e_mapping_level decrease_mapping_level(e_mapping_level level) {
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

e_sequence_level decrease_sequence_level(e_sequence_level level) {
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
    if (route == NULL) {
        return;
    }
    if (route->id != NULL) {
        log_debug("free route: %s", route->id);
        free(route->id);
        route->id = NULL;
    }
    if (route->source != NULL) {
        log_debug("\tfree route source");
        free(route->source);
        route->source = NULL;
    }
    if (route->destination != NULL) {
        log_debug("\tfree route destination");
        free(route->destination);
        route->destination = NULL;
    }
    if (route->orientation != NULL) {
        log_debug("\tfree route orientation");
        free(route->orientation);
        route->orientation = NULL;
    }
    if (route->train != NULL) {
        log_debug("\tfree route train");
        free(route->train);
        route->train = NULL;
    }

    if (route->path != NULL) {
        log_debug("\tfree route path");
        for (int i = 0; i < route->path->len; ++i) {
            free(g_array_index(route->path, char *, i));
        }
        g_array_free(route->path, true);
    }

    if (route->sections != NULL) {
        log_debug("\tfree route sections");
        for (int i = 0; i < route->sections->len; ++i) {
            free(g_array_index(route->sections, char *, i));
        }
        g_array_free(route->sections, true);
    }

    if (route->points != NULL) {
        log_debug("\tfree route points");
        //differently allocated than other g_arrays.
        g_array_free(route->points, true);
    }

    if (route->signals != NULL) {
        log_debug("\tfree route signals");
        for (int i = 0; i < route->signals->len; ++i) {
            free(g_array_index(route->signals, char *, i));
        }
        g_array_free(route->signals, true);
    }

    if (route->conflicts != NULL) {
        log_debug("\tfree route conflicts");
        for (int i = 0; i < route->conflicts->len; ++i) {
            free(g_array_index(route->conflicts, char *, i));
        }
        g_array_free(route->conflicts, true);
    }
    free(route);
}

void free_interlocking_point(void *item) {
    t_interlocking_point *point = (t_interlocking_point *) item;
    if (point == NULL) {
        return;
    }
    if (point->id != NULL) {
        log_debug("free interlocking point: %s", point->id);
        free(point->id);
        point->id = NULL;
    }
    // free(point) is not necessary as it is not malloced for the hashtable
}

GHashTable *parse(yaml_parser_t *parser) {
    e_mapping_level cur_mapping = MAPPING_TABLE;
    e_sequence_level cur_sequence = SEQUENCE_NONE;

    GHashTable *routes = NULL;
    t_interlocking_route *route = NULL;
    t_interlocking_point *point = NULL;

    yaml_event_t event;
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

        switch (event.type) {
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
                    } else if (is_str_equal(last_scalar, "sections")) {
                        route->sections = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_SECTIONS;
                    } else if (is_str_equal(last_scalar, "points")) {
                        route->points = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_point), 8);
                        g_array_set_clear_func(route->points, free_interlocking_point);
                        cur_sequence = SEQUENCE_POINTS;
                    } else if (is_str_equal(last_scalar, "signals")) {
                        route->signals = g_array_sized_new(FALSE, TRUE, sizeof(char *), 8);
                        cur_sequence = SEQUENCE_SIGNALS;
                    } else if (is_str_equal(last_scalar, "conflicts")) {
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
                    if (route == NULL) {
                        log_debug("interlocking parser parse: failed to allocate memory for route");
                        exit(1);
                    }
                    route->id = NULL;
                    route->source = NULL;
                    route->destination = NULL;
                    route->orientation = NULL;
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
                } else if (cur_sequence == SEQUENCE_SECTIONS) {
                    cur_mapping = MAPPING_BLOCK;
                } else if (cur_sequence == SEQUENCE_POINTS) {
                    cur_mapping = MAPPING_POINT;
                    g_array_append_val(route->points, (t_interlocking_point){});
                    point = &g_array_index(route->points, t_interlocking_point, route->points->len - 1);
                } else if (cur_sequence == SEQUENCE_SIGNALS) {
                    cur_mapping = MAPPING_SIGNAL;
                } else if (cur_sequence == SEQUENCE_CONFLICTS) {
                    cur_mapping = MAPPING_CONFLICT;
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
                cur_scalar = strdup((char *)event.data.scalar.value);

                if (last_scalar == NULL) {
                    break;
                }

                // route
                if (cur_mapping == MAPPING_ROUTE) {
                    if (is_str_equal(last_scalar, "id")) {
                        route->id = strdup(cur_scalar);
                    } else if (is_str_equal(last_scalar, "source")) {
                        route->source = strdup(cur_scalar);
                    } else if (is_str_equal(last_scalar, "destination")) {
                        route->destination = strdup(cur_scalar);
                    } else if (is_str_equal(last_scalar, "orientation")) {
                        route->orientation = strdup(cur_scalar);
                    } else if (is_str_equal(last_scalar, "length")) {
                        route->length = strtof(cur_scalar, NULL);
                    }
                } else if (cur_mapping == MAPPING_SEGMENT) {
                    // segment
                    if (is_str_equal(last_scalar, "id")) {
                        char *tmp_scalar = strdup(cur_scalar);
                        g_array_append_val(route->path, tmp_scalar);
                    }
                } else if (cur_mapping == MAPPING_BLOCK) {
                    // block
                    if (is_str_equal(last_scalar, "id")) {
                        char *tmp_scalar = strdup(cur_scalar);
                        g_array_append_val(route->sections, tmp_scalar);
                    }
                } else if (cur_mapping == MAPPING_POINT) {
                    // point
                    if (is_str_equal(last_scalar, "id")) {
                        point->id = strdup(cur_scalar);
                    } else if (is_str_equal(last_scalar, "position")) {
                        point->position = is_str_equal(cur_scalar, "reverse")
                                          ? REVERSE
                                          : NORMAL;
                    }
                } else if (cur_mapping == MAPPING_SIGNAL) {
                    // signal
                    if (is_str_equal(last_scalar, "id")) {
                        char *tmp_scalar = strdup(cur_scalar);
                        g_array_append_val(route->signals, tmp_scalar);
                    }
                } else if (cur_mapping == MAPPING_CONFLICT) {
                    // conflict
                    if (is_str_equal(last_scalar, "id")) {
                        char *tmp_scalar = strdup(cur_scalar);
                        g_array_append_val(route->conflicts, tmp_scalar);
                    }
                }

                break;
            default:
                error = true;
                break;
        }
        
        yaml_event_delete(&event);
        
        if (last_scalar != NULL && last_scalar != cur_scalar) {
            free(last_scalar);
            last_scalar = NULL;
        }
        if (cur_scalar != NULL) {
            last_scalar = strdup(cur_scalar);
            free(cur_scalar);
            cur_scalar = NULL;
        } else {
            last_scalar = cur_scalar;
        }
    } while (!error);
    
    if (cur_scalar != NULL) {
        free(cur_scalar);
        cur_scalar = NULL;
    }
    if (last_scalar != NULL) {
        free(last_scalar);
        last_scalar = NULL;
    }
    
    return routes;
}

GHashTable *parse_interlocking_table(const char *config_dir) {
    if (config_dir == NULL) {
        syslog_server(LOG_ERR, "Interlocking parser: config directory is missing");
        return false;
    }

    // init
    FILE *fh;
    yaml_parser_t parser;
    if (!init_parser(config_dir, INTERLOCKING_TABLE_FILENAME, &fh, &parser)) {
        syslog_server(LOG_ERR, "Interlocking parser: Interlocking table file is missing");
        return false;
    }

    // parse
    GHashTable *routes = parse(&parser);

    // clean
    yaml_parser_delete(&parser);
    fclose(fh);

    // success
    if (routes != NULL) {
        syslog_server(LOG_INFO, "Interlocking parser: Interlocking table loaded successfully: %d routes", 
                      g_hash_table_size(routes));
        return routes;
    }

    // error
    syslog_server(LOG_ERR, "Interlocking parser: Failed to load interlocking table");
    return NULL;
}
