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

#include <bidib.h>
#include <string.h>

#include "server.h"
#include "interlocking.h"
#include "parsers/config_data_parser.h"
#include "parsers/config_data_intern.h"
#include "bahn_data_util.h"
#include "handler_driver.h"

t_config_data config_data = {};

int get_route_array_string_value(t_interlocking_route *route, const char *prop_name, char **data);

typedef enum {
    TYPE_ROUTE,
    TYPE_SEGMENT,
    TYPE_SIGNAL,
    TYPE_POINT,
    TYPE_TRAIN,
    TYPE_BLOCK,
    TYPE_CROSSING,
    TYPE_SIGNAL_TYPE,
    TYPE_COMPOSITE_SIGNAL,
    TYPE_NOT_SUPPORTED
} e_config_type;

bool bahn_data_util_initialise_config(const char *config_dir) {

    if (!interlocking_table_initialise(config_dir)) {
        return false;
    }

    if (!parse_config_data(config_dir, &config_data)) {
        return false;
    }

    return true;
}

void bahn_data_util_free_config() {
    free_config_data(config_data);
    free_interlocking_table();
}

bool string_equals(const char *str1, const char *str2) {
    return strcmp(str1, str2) == 0;
}

GArray *cached_allocated_str;

void bahn_data_util_init_cached_track_state() {
    cached_allocated_str = g_array_sized_new(FALSE, FALSE, sizeof(char *), 16);
}

void bahn_data_util_free_cached_track_state() {
    if (cached_allocated_str != NULL) {
        g_array_free(cached_allocated_str, true);
        cached_allocated_str = NULL;
    }
}

void add_cache_str(char *state) {
    if (cached_allocated_str != NULL) {
        g_array_append_val(cached_allocated_str, state);
    }
}

char *new_empty_str() {
    char *result = strdup("");
    g_array_append_val(cached_allocated_str, result);
    return result;
}

e_config_type get_config_type(const char *type) {
    if (string_equals(type, "route")) {
        return TYPE_ROUTE;
    }

    if (string_equals(type, "segment")) {
        return TYPE_SEGMENT;
    }

    if (string_equals(type, "signal")) {
        return TYPE_SIGNAL;
    }

    if (string_equals(type, "point")) {
        return TYPE_POINT;
    }

    if (string_equals(type, "train")) {
        return TYPE_TRAIN;
    }

    if (string_equals(type, "block")) {
        return TYPE_BLOCK;
    }

    if (string_equals(type, "crossing")) {
        return TYPE_CROSSING;
    }

    if (string_equals(type, "signaltype")) {
        return TYPE_SIGNAL_TYPE;
    }

    return TYPE_NOT_SUPPORTED;
}

void *get_object(e_config_type config_type, const char *id) {
    GHashTable *tb = NULL;
    switch (config_type) {
        case TYPE_ROUTE:
            return get_route((int)strtol(id, NULL, 10));
        case TYPE_SEGMENT:
            tb = config_data.table_segments;
            break;
        case TYPE_SIGNAL:
            tb = config_data.table_signals;
            break;
        case TYPE_POINT:
            tb = config_data.table_points;
            break;
        case TYPE_TRAIN:
            tb = config_data.table_trains;
            break;
        case TYPE_BLOCK:
            tb = config_data.table_blocks;
            break;
        case TYPE_CROSSING:
            tb = config_data.table_crossings;
            break;
        case TYPE_SIGNAL_TYPE:
            tb = config_data.table_signal_types;
            break;
        case TYPE_COMPOSITE_SIGNAL:
            tb = config_data.table_composite_signals;
            break;
        default:
            break;
    }

    // check
    if (tb != NULL) {
        if (g_hash_table_contains(tb, id)) {
            return g_hash_table_lookup(tb, id);
        }
    }

    return NULL;
}

int interlocking_table_get_routes(const char *src_signal_id, const char *dst_signal_id, char *route_ids[]) {
    GArray *arr = interlocking_table_get_route_ids(src_signal_id, dst_signal_id);
    if (arr != NULL) {
        for (int i = 0; i < arr->len; ++i) {
            route_ids[i] = g_array_index(arr, char *, i);
        }
        return arr->len;
    }

    return 0;
}

char *config_get_scalar_string_value(const char *type, const char *id, const char *prop_name) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    char *result = NULL;
    if (obj != NULL) {
        switch (config_type) {
            case TYPE_ROUTE:
                if (string_equals(prop_name, "id")) {
                    result = ((t_interlocking_route *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "source")) {
                    result = ((t_interlocking_route *) obj)->source;
                    break;
                }

                if (string_equals(prop_name, "destination")) {
                    result = ((t_interlocking_route *) obj)->destination;
                    break;
                }

                if (string_equals(prop_name, "train")) {
                    result = ((t_interlocking_route *) obj)->train;
                    break;
                }

                break;
                
            case TYPE_SEGMENT:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_segment *) obj)->id;
                    break;
                }

                break;
                
            case TYPE_SIGNAL:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_signal *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "initial")) {
                    result = ((t_config_signal *) obj)->initial;
                    break;
                }

                if (string_equals(prop_name, "type")) {
                    result = ((t_config_signal *) obj)->type;
                    break;
                }

                break;
                
            case TYPE_POINT:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_point *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "initial")) {
                    result = ((t_config_point *) obj)->initial;
                    break;
                }

                if (string_equals(prop_name, "segment")) {
                    result = ((t_config_point *) obj)->segment;
                    break;
                }

                if (string_equals(prop_name, "normal")) {
                    result = ((t_config_point *) obj)->normal_aspect;
                    break;
                }

                if (string_equals(prop_name, "reverse")) {
                    result = ((t_config_point *) obj)->reverse_aspect;
                    break;
                }

                break;
                
            case TYPE_TRAIN:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_train *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "type")) {
                    result = ((t_config_train *) obj)->type;
                    break;
                }

                break;
                
            case TYPE_BLOCK:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_block *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "segment")) {
                    result = ((t_config_block *) obj)->main_segment;
                    break;
                }

                if (string_equals(prop_name, "direction")) {
                    result = ((t_config_block *) obj)->direction;
                    break;
                }

                break;
                
            case TYPE_CROSSING:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_crossing *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "segment")) {
                    result = ((t_config_crossing *) obj)->main_segment;
                    break;
                }

                break;
                
            case TYPE_SIGNAL_TYPE:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_signal_type *) obj)->id;
                    break;
                }

                break;

            case TYPE_COMPOSITE_SIGNAL:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_composite_signal *) obj)->id;
                    break;
                }

                if (string_equals(prop_name, "entry")) {
                    result = ((t_config_composite_signal *) obj)->entry;
                    break;
                }

                if (string_equals(prop_name, "exit")) {
                    result = ((t_config_composite_signal *) obj)->exit;
                    break;
                }

                if (string_equals(prop_name, "block")) {
                    result = ((t_config_composite_signal *) obj)->block;
                    break;
                }

                if (string_equals(prop_name, "distant")) {
                    result = ((t_config_composite_signal *) obj)->distant;
                    break;
                }

                break;
                
            default:
                break;
        }
    }

    result = result != NULL ? result : new_empty_str();
    syslog_server(LOG_DEBUG, "Get scalar string: %s %s.%s => %s", type, id, prop_name, result);
    return result;
}

int config_get_scalar_int_value(const char *type, const char *id, const char *prop_name) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    int result = 0;
    if (obj != NULL && config_type == TYPE_BLOCK) {
        if (string_equals(prop_name, "limit")) {
            result = ((t_config_block *) obj)->limit_speed;
        }
    }

    syslog_server(LOG_DEBUG, "Get scalar int: %s %s.%s => %.2f", type, id, prop_name, result);
    return result;
}

float config_get_scalar_float_value(const char *type, const char *id, const char *prop_name) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    float result = 0;
    if (obj != NULL) {
        switch (config_type) {
            case TYPE_ROUTE:
                if (string_equals(prop_name, "length")) {
                    result = ((t_interlocking_route *) obj)->length;
                }
                break;
                
            case TYPE_BLOCK:
                if (string_equals(prop_name, "length")) {
                    result = ((t_config_block *) obj)->length;
                    break;
                }
                break;

            case TYPE_SEGMENT:
                if (string_equals(prop_name, "length")) {
                    result = ((t_config_segment *) obj)->length;
                }
                break;
                
            case TYPE_TRAIN:
                if (string_equals(prop_name, "length")) {
                    result = ((t_config_train *) obj)->length;
                    break;
                }

                if (string_equals(prop_name, "weight")) {
                    result = ((t_config_train *) obj)->weight;
                }
                break;
                
            default:
                break;
        }
    }

    syslog_server(LOG_DEBUG, "Get scalar float: %s %s.%s => %.2f", type, id, prop_name, result);
    return result;
}

bool config_get_scalar_bool_value(const char *type, const char *id, const char *prop_name) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    bool result = false;
    if (obj != NULL) {
		switch (config_type) {
			case TYPE_BLOCK:
                if (string_equals(prop_name, "reversed")) {
                    result = ((t_config_block *) obj)->reversed;
                    break;
                }
				break;
				
            default:
                break;
		}
    }
    
    return result;
}

int config_get_array_string_value(const char *type, const char *id, const char *prop_name, char* data[]) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    int result = 0;
    if (obj != NULL) {
        GArray *arr = NULL;
        switch (config_type) {
            case TYPE_ROUTE:
                result = get_route_array_string_value((t_interlocking_route *)obj, prop_name, data);
                break;
                
            case TYPE_SIGNAL:
                if (string_equals(prop_name, "aspects")) {
                    arr = ((t_config_signal *) obj)->aspects;
                    break;
                }
                
                break;
                
            case TYPE_TRAIN:
                if (string_equals(prop_name, "peripherals")) {
                    arr = ((t_config_train *) obj)->peripherals;
                    break;
                }
				
				break;
				
            case TYPE_BLOCK:
                if (string_equals(prop_name, "train_types")) {
                    arr = ((t_config_block *) obj)->train_types;
                    break;
                }

                if (string_equals(prop_name, "block_signals")) {
                    arr = ((t_config_block *) obj)->signals;
                    break;
                }

                if (string_equals(prop_name, "overlaps")) {
                    arr = ((t_config_block *) obj)->overlaps;
                    break;
                }
				break;
				
            case TYPE_SIGNAL_TYPE:
                if (string_equals(prop_name, "aspects")) {
                    arr = ((t_config_signal_type *) obj)->aspects;
                    break;
                }
                
                break;
                
            default:
                break;
        }

        if (arr != NULL) {
            for (int i = 0; i < arr->len; ++i) {
                data[i] = g_array_index(arr, char *, i);
            }
            result = arr->len;
        }
    }

    syslog_server(LOG_DEBUG, "Get array string: %s %s.%s => %d", type, id, prop_name, result);
    return result;
}

int get_route_array_string_value(t_interlocking_route *route, const char *prop_name, char* data[]) {
    if (string_equals(prop_name, "path")) {
        if (route->path != NULL) {
            for (int i = 0; i < route->path->len; ++i) {
                data[i] = g_array_index(route->path, char *, i);
            }
            return route->path->len;
        }
    }

    if (string_equals(prop_name, "sections")) {
        if (route->sections != NULL) {
            for (int i = 0; i < route->sections->len; ++i) {
                data[i] = g_array_index(route->sections, char *, i);
            }
            return route->sections->len;
        }
    }

    if (string_equals(prop_name, "point_positions")) {
        if (route->points != NULL) {
            for (int i = 0; i < route->points->len; ++i) {
                data[i] = (&g_array_index(route->points, t_interlocking_point, i))->id;
            }
            return route->points->len;
        }
    }

    if (string_equals(prop_name, "route_signals")) {
        if (route->signals != NULL) {
            for (int i = 0; i < route->signals->len; ++i) {
                data[i] = g_array_index(route->signals, char *, i);
            }
            return route->signals->len;
        }
    }

    if (string_equals(prop_name, "conflicts")) {
        if (route->conflicts != NULL) {
            for (int i = 0; i < route->conflicts->len; ++i) {
                data[i] = g_array_index(route->conflicts, char *, i);
            }
            return route->conflicts->len;
        }
    }

    return 0;
}

int config_get_array_int_value(const char *type, const char *id, const char *prop_name, int data[]) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    int result = 0;
    if (obj != NULL) {
        GArray *arr = NULL;
        switch (config_type) {
            case TYPE_TRAIN:
                if (string_equals(prop_name, "calibration")) {
                    arr = ((t_config_train *) obj)->calibration;
                    break;
                }
            default:
                break;
        }

        if (arr != NULL) {
            for (int i = 0; i < arr->len; ++i) {
                data[i] = g_array_index(arr, int, i);
            }
            result = arr->len;
        }
    }

    syslog_server(LOG_DEBUG, "Get array int: %s %s.%s => %d", type, id, prop_name, result);
    return result;
}

int config_get_array_float_value(const char *type, const char *id, const char *prop_name, float data[]) {
    // No object type has float array property
    return 0;
}

int config_get_array_bool_value(const char *type, const char *id, const char *prop_name, bool data[]) {
    // No object type has bool array property
    return 0;
}

bool config_set_scalar_string_value(const char *type, const char *id, const char *prop_name, char *value) {
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    bool result = false;
    if (obj != NULL && config_type == TYPE_ROUTE) {
        if (string_equals(prop_name, "train")) {
            // Set train
            t_interlocking_route *route = (t_interlocking_route *) obj;
            route->train = strdup(value);
            result = true;
        }
    }

    syslog_server(LOG_DEBUG, "Set scalar string: %s %s.%s = %s => %s", type, id, prop_name, value, result ? "true" : "false");
    return result;
}

e_config_type get_track_state_type(const char *id) {
    if (id == NULL)
        return TYPE_NOT_SUPPORTED;

    if (g_hash_table_contains(config_data.table_signals, id)) {
        return TYPE_SIGNAL;
    }

    if (g_hash_table_contains(config_data.table_points, id)) {
        return TYPE_POINT;
    }

    return TYPE_NOT_SUPPORTED;
}

bool set_signal_raw_aspect(t_config_signal *signal, const char *value) {
    if (signal->aspects != NULL) {
        for (int i = 0; i < signal->aspects->len; ++i) {
            char *aspect = g_array_index(signal->aspects, char *, i);
            if (string_equals(aspect, value)) {
                return bidib_set_signal(signal->id, value) == 0;
            }
        }
    }

    return false;
}

/**
 * Get raw signal aspect from bidib state
 * Convert back to signalling action based on signal type
 * caution action is not completely supported yet because it requires multiple aspects enabled
 * @param id signal name
 * @param value stop, clear or caution
 * @return true of success, otherwise false
 */
char *get_signal_state(const char *id) {
    t_config_signal *signal = get_object(TYPE_SIGNAL, id);
    if (signal == NULL)
        return "";

    const char *type = signal->type;

    // load raw state
    char *raw_state = NULL;
    t_bidib_unified_accessory_state_query state_query = bidib_get_signal_state(id);
    if (state_query.known) {
        raw_state = strdup(state_query.board_accessory_state.state_id);
    }
    bidib_free_unified_accessory_state_query(state_query);

    // load signalling aspect based on signal type
    char *result = NULL;
    if (string_equals(raw_state, "red")) {
        if (string_equals(type, "entry")
            || string_equals(type, "exit")
            || string_equals(type, "block")
            || string_equals(type, "stoplight")) {

            result = "stop";
        }
    } else if (string_equals(raw_state, "green")){
        if (string_equals(type, "entry")
            || string_equals(type, "exit")
            || string_equals(type, "block")
            || string_equals(type, "distant")) {

            result = "clear";
        }
    } else if (string_equals(raw_state, "yellow")){
        if (string_equals(type, "distant")) {
            result = "stop";
        } else if (string_equals(type, "entry")
                   || string_equals(type, "exit")) {
            result = "caution";
        }
    } else if (string_equals(raw_state, "white")){
        if (string_equals(type, "stoplight")) {
            result = "clear";
        }
    }

    // free
    if (raw_state != NULL) {
        free(raw_state);
    }

    return result;
}

/**
 * Convert the signalling action to raw aspect based on signal types
 * Update bidib state
 * @param id signal name
 * @param value stop, clear or caution
 * @return true of success, otherwise false
 */
bool set_signal_state(const char *id, const char *value) {
    t_config_signal *signal = get_object(TYPE_SIGNAL, id);
    if (signal == NULL)
        return false;

    if (string_equals(value, "stop")) {

        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "block")
            || string_equals(signal->type, "stoplight")) {

            return set_signal_raw_aspect(signal, "red");
        }

        if (string_equals(signal->type, "distant")) {
            return set_signal_raw_aspect(signal, "yellow");
        }

        return false;
    }

    if (string_equals(value, "clear")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "block")
            || string_equals(signal->type, "distant")) {

            return set_signal_raw_aspect(signal, "green");
        }

        if (string_equals(signal->type, "stoplight")) {
            return set_signal_raw_aspect(signal, "white");
        }

        return false;
    }

    if (string_equals(value, "caution")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "distant")) {

            return set_signal_raw_aspect(signal, "yellow") && set_signal_raw_aspect(signal, "green");
        }

        return false;
    }

    return false;
}

char *track_state_get_value(const char *id) {
    char *result = NULL;
    e_config_type config_type = get_track_state_type(id);
    void *obj = get_object(config_type, id);
    if (obj != NULL) {
        if (config_type == TYPE_POINT) {
            t_bidib_unified_accessory_state_query state_query = bidib_get_point_state(id);
            if (state_query.known) {
                result = strdup(state_query.board_accessory_state.state_id);
            }
            bidib_free_unified_accessory_state_query(state_query);
        } else if (config_type == TYPE_SIGNAL){
            result = get_signal_state(id);
        }
    }

    if (result != NULL) {
        add_cache_str(result);
    } else {
        result = new_empty_str();
    }

    syslog_server(LOG_DEBUG, "Get track state: %s => %s", id, result);
    return result;
}

bool track_state_set_value(const char *id, const char *value) {
    e_config_type config_type = get_track_state_type(id);
    bool result = false;
    switch (config_type) {
        case TYPE_POINT:
            if (string_equals(value, "normal") || string_equals(value, "reverse")) {
                result = bidib_switch_point(id, value) == 0;
                syslog_server(LOG_DEBUG, "Set point state: %s to %s => %s", id, value, result ? "true" : "false");
            } else {
                syslog_server(LOG_DEBUG, "Invalid point state: %s", value);
            }
            return result;
        case TYPE_SIGNAL:
            result = set_signal_state(id, value);
            syslog_server(LOG_DEBUG, "Set signal state: %s to %s => %s", id, value, result ? "true" : "false");
            return result;
        default:
            return false;
    }
}

bool is_segment_occupied(const char *id) {
    bool result = false;
    if (g_hash_table_contains(config_data.table_segments, id)) {
        t_bidib_segment_state_query state_query = bidib_get_segment_state(id);
        result = state_query.known && state_query.data.occupied;
        bidib_free_segment_state_query(state_query);
    }

    syslog_server(LOG_DEBUG, "Is segment occupied: %s => %s", id, result ? "true" : "false");
    return result;
}

int train_state_get_speed(const char *train_id) {
    int result = 0;
    if (g_hash_table_contains(config_data.table_trains, train_id)) {
        t_bidib_train_speed_kmh_query kmh_query = bidib_get_train_speed_kmh(train_id);
        if (kmh_query.known_and_avail) {
            result = kmh_query.speed_kmh;
        }
    } else {
        syslog_server(LOG_ERR, "Invalid train id: %s", train_id);
    }

    syslog_server(LOG_DEBUG, "Get train speed in km/h: %s => %d", train_id, result);
    return result;
}

bool train_state_set_speed(const char *train_id, int speed) {
    bool result = false;
    if (g_hash_table_contains(config_data.table_trains, train_id)) {
        const int grab_id = train_get_grab_id(train_id);
        bidib_set_train_speed(train_id, speed, grabbed_trains[grab_id].track_output);
    } else {
        syslog_server(LOG_ERR, "Invalid train id: %s", train_id);
    }

    syslog_server(LOG_DEBUG, "Set train speed in km/h: %s to %d => %s", train_id, speed, result ? "true" : "false");
    return result;
}

char *config_get_point_position(const char *route_id, const char *point_id) {
    void *obj = get_object(TYPE_ROUTE, route_id);
    char *result = NULL;

    if (obj != NULL) {
        t_interlocking_route *route = (t_interlocking_route *) obj;
        for (int i = 0; i < route->points->len; ++i) {
            t_interlocking_point *point = &g_array_index(route->points, t_interlocking_point, i);
            if (string_equals(point->id, point_id)) {
                result = point->position == NORMAL ? "normal" : "reverse";
                break;
            }
        }
    }

    result = result != NULL ? result : new_empty_str();
    syslog_server(LOG_DEBUG, "Get route point position: %s.%s => %s", route_id, point_id, result);
    return result;
}
