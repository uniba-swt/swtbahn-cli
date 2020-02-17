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
#include "interlocking.h"
#include "server.h"

#include <stdio.h>
#include <stdbool.h>
#include <yaml.h>

const char INTERLOCKING_FILE_NAME[]= "interlocking_table.yml";
GArray *parse(yaml_parser_t *parser);


bool is_str_equal(const char *str1, const char *str2) {
	return strcmp(str1, str2) == 0;
}

long convert_long_value(const char *str) {
	char *ptr;
	return strtol(str, &ptr, 10);
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
		syslog_server(LOG_ERR, "Interlocking parser: Failed to open %s", table_file);
		return false;
	}

	if (!yaml_parser_initialize(parser)) {
		fclose(*fh);
		syslog_server(LOG_ERR, "Interlocking parser: Failed to initialise parser");
		return false;
	}

	yaml_parser_set_input_file(parser, *fh);
	return true;
}

typedef enum {
	SEQUENCE_NONE,
	SEQUENCE_ROUTES,
	SEQUENCE_PATH,
	SEQUENCE_POINTS,
	SEQUENCE_SIGNALS,
	SEQUENCE_CONFLICTS
} e_sequence_level;

typedef enum {
	MAPPING_TABLE,
	MAPPING_ROUTE,
	MAPPING_SEGMENT,
	MAPPING_POINT,
	MAPPING_SIGNAL,
	MAPPING_CONFLICT
} e_mapping_level;

e_mapping_level decrease_mapping_level(e_mapping_level level) {
	switch (level) {
		case MAPPING_ROUTE:
			return MAPPING_TABLE;
		case MAPPING_SEGMENT:
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
		case SEQUENCE_POINTS:
		case SEQUENCE_SIGNALS:
		case SEQUENCE_CONFLICTS:
			return SEQUENCE_ROUTES;
		default:
			return level;
	}
}

void free_route(void *item) {
	t_interlocking_route *route = (t_interlocking_route *) item;
	free(route->source.id);
	free(route->destination.id);
	
	if (route->path != NULL) {
		g_array_free(route->path, true);
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

void free_route_path(void *item) {
	t_interlocking_path_segment *path = (t_interlocking_path_segment *) item;
	free(path->id);
}

void free_route_point(void *item) {
	t_interlocking_point *point = (t_interlocking_point *)item;
	free(point->id);
}

void free_route_signal(void *item) {
	t_interlocking_signal *signal = (t_interlocking_signal *)item;
	free(signal->id);
}

GArray *parse(yaml_parser_t *parser) {
	e_mapping_level cur_mapping = MAPPING_TABLE;
	e_sequence_level cur_sequence = SEQUENCE_NONE;

	GArray *routes = NULL;
	t_interlocking_route *route = NULL;
	t_interlocking_path_segment *segment = NULL;
	t_interlocking_point *point = NULL;
	t_interlocking_signal *signal = NULL;

	yaml_event_t  event;
	bool error = false;
	char *cur_scalar = NULL;
	char *last_scalar = NULL;

	do {
		if (!yaml_parser_parse(parser, &event)) {
			syslog_server(LOG_ERR, "Interlocking parser: error %d\n", (*parser).error);
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
						routes = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_route), 64);
						g_array_set_clear_func(routes, free_route);
                    }
					break;
				}

				// array member of route
				if (cur_mapping == MAPPING_ROUTE) {
					if (is_str_equal(last_scalar, "path")) {
						route->path = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_path_segment), 8);
						g_array_set_clear_func(route->path, free_route_path);
						cur_sequence = SEQUENCE_PATH;
						break;
					}
					
					if (is_str_equal(last_scalar, "points")) {
						route->points = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_point), 8);
						g_array_set_clear_func(route->points, free_route_point);
						cur_sequence = SEQUENCE_POINTS;
						break;
					}
					
					if (is_str_equal(last_scalar, "signals")) {
						route->signals = g_array_sized_new(FALSE, TRUE, sizeof(t_interlocking_signal), 8);
						g_array_set_clear_func(route->signals, free_route_signal);
						cur_sequence = SEQUENCE_SIGNALS;
						break;
					}
					
					if (is_str_equal(last_scalar, "conflicts")) {
						route->conflicts = g_array_sized_new(FALSE, FALSE, sizeof(size_t), 8);
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
					g_array_append_val(routes, (t_interlocking_route){});
					route = &g_array_index(routes, t_interlocking_route, routes->len - 1);
					break;
				}

				// path -> create segment
				if (cur_sequence == SEQUENCE_PATH) {
					cur_mapping = MAPPING_SEGMENT;
					g_array_append_val(route->path, (t_interlocking_path_segment){
						.bidib_state_index = -1
					});
					segment = &g_array_index(route->path, t_interlocking_path_segment, route->path->len - 1);
					break;
				}

				if (cur_sequence == SEQUENCE_POINTS) {
					cur_mapping = MAPPING_POINT;
					g_array_append_val(route->points, (t_interlocking_point){
						.bidib_state_index = -1
					});
					point = &g_array_index(route->points, t_interlocking_point, route->points->len - 1);
					break;
				}

				if (cur_sequence == SEQUENCE_SIGNALS) {
					cur_mapping = MAPPING_SIGNAL;
					g_array_append_val(route->signals, (t_interlocking_signal){
						.bidib_state_index = -1
					});
					signal = &g_array_index(route->signals, t_interlocking_signal , route->signals->len - 1);
					break;
				}

				if (cur_sequence == SEQUENCE_CONFLICTS) {
					cur_mapping = MAPPING_CONFLICT;
					break;
				}
				break;
			case YAML_MAPPING_END_EVENT:
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
						route->id = convert_long_value(cur_scalar);
						break;
					}

					if (is_str_equal(last_scalar, "source")) {
						route->source.id = cur_scalar;
						route->source.bidib_state_index = -1;
						break;
					}

					if (is_str_equal(last_scalar, "destination")) {
						route->destination.id = cur_scalar;
						route->destination.bidib_state_index = -1;
						break;
					}

					if (is_str_equal(last_scalar, "direction")) {
						route->direction = is_str_equal(cur_scalar, "clockwise")
						                   ? CLOCKWISE
						                   : ANTICLOCKWISE;
					}

					break;
				}

				// segment
				if (cur_mapping == MAPPING_SEGMENT) {
					if (is_str_equal(last_scalar, "id")) {
						segment->id = cur_scalar;
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
						signal->id = cur_scalar;
					}

					break;
				}

				// conflict
				if (cur_mapping == MAPPING_CONFLICT) {
					if (is_str_equal(last_scalar, "id")) {
						size_t id = convert_long_value(cur_scalar);
						g_array_append_val(route->conflicts, id);
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

GArray *parse_interlocking_table(const char *config_dir) {
	if (config_dir == NULL) {
		syslog_server(LOG_ERR, "Interlocking parser: Missing configuration directory");
		return NULL;
	}

	// init
	FILE *fh;
	yaml_parser_t parser;
	if (!init_parser(config_dir, INTERLOCKING_FILE_NAME, &fh, &parser)) {
		return NULL;
	}

	// parse
	GArray *routes = parse(&parser);

	// clean
	yaml_parser_delete(&parser);
	fclose(fh);

	if (routes != NULL) {
		// success
		syslog_server(LOG_INFO, "Interlocking parser: Interlocking table parsed");
		return routes;
	} else {
		// error
		syslog_server(LOG_ERR, "Interlocking parser: Failed to parse interlocking table");
		return NULL;
	}
}
