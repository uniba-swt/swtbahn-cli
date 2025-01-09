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

#include <bidib/bidib.h>
#include <string.h>

#include "server.h"
#include "interlocking.h"
#include "parsers/config_data_parser.h"
#include "parsers/config_data_intern.h"
#include "bahn_data_util.h"
#include "handler_driver.h"

typedef enum {
    TYPE_ROUTE,
    TYPE_SEGMENT,
    TYPE_REVERSER,
    TYPE_SIGNAL,
    TYPE_POINT,
    TYPE_PERIPHERAL,
    TYPE_TRAIN,
    TYPE_BLOCK,
    TYPE_CROSSING,
    TYPE_SIGNAL_TYPE,
    TYPE_PERIPHERAL_TYPE,
    TYPE_COMPOSITE_SIGNAL,
    TYPE_NOT_SUPPORTED
} e_config_type;

t_config_data config_data = {};

// Needed to temporarily store new strings created by track_state_get_value
GArray *cached_allocated_str_array = NULL;

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
    if (str1 != NULL && str2 != NULL) {
        return strcmp(str1, str2) == 0;
    } else {
        return false;
    }
}

void bahn_data_util_init_cached_track_state() {
    if (cached_allocated_str_array != NULL) {
        syslog_server(LOG_ERR, 
                      "bahn data util init cached track state - cache is not NULL, "
                      "either concurrent usage or someone didn't free the cache correctly!");
        return;
    }
    cached_allocated_str_array = g_array_sized_new(FALSE, FALSE, sizeof(char *), 16);
}

void bahn_data_util_free_cached_track_state() {
    if (cached_allocated_str_array != NULL) {
        for (int i = 0; i < cached_allocated_str_array->len; ++i) {
            free(g_array_index(cached_allocated_str_array, char *, i));
        }
        g_array_free(cached_allocated_str_array, true);
        cached_allocated_str_array = NULL;
    }
}

void add_cache_str(char *state) {
    if (cached_allocated_str_array != NULL) {
        g_array_append_val(cached_allocated_str_array, state);
    } else {
        syslog_server(LOG_ERR, "bahn data util add cache str - cache is NULL!");
    }
}

e_config_type get_config_type(const char *type) {
    if (string_equals(type, "route")) {
        return TYPE_ROUTE;
    } else if (string_equals(type, "segment")) {
        return TYPE_SEGMENT;
    } else if (string_equals(type, "reverser")) {
        return TYPE_REVERSER;
    } else if (string_equals(type, "signal")) {
        return TYPE_SIGNAL;
    } else if (string_equals(type, "point")) {
        return TYPE_POINT;
    } else if (string_equals(type, "peripheral")) {
        return TYPE_PERIPHERAL;
    } else if (string_equals(type, "train")) {
        return TYPE_TRAIN;
    } else if (string_equals(type, "block")) {
        return TYPE_BLOCK;
    } else if (string_equals(type, "crossing")) {
        return TYPE_CROSSING;
    } else if (string_equals(type, "composition")) {
        return TYPE_COMPOSITE_SIGNAL;
    } else if (string_equals(type, "signaltype")) {
        return TYPE_SIGNAL_TYPE;
    } else if (string_equals(type, "peripheraltype")) {
        return TYPE_PERIPHERAL_TYPE;
    }

    return TYPE_NOT_SUPPORTED;
}

void *get_object(e_config_type config_type, const char *id) {
    GHashTable *tb = NULL;
    switch (config_type) {
        case TYPE_ROUTE:
            return get_route(id);
        case TYPE_SEGMENT:
            tb = config_data.table_segments;
            break;
        case TYPE_REVERSER:
            tb = config_data.table_reversers;
            break;
        case TYPE_SIGNAL:
            tb = config_data.table_signals;
            break;
        case TYPE_POINT:
            tb = config_data.table_points;
            break;
        case TYPE_PERIPHERAL:
            tb = config_data.table_peripherals;
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
        case TYPE_PERIPHERAL_TYPE:
            tb = config_data.table_peripheral_types;
            break;
        default:
            break;
    }

    if (tb != NULL) {
        if (g_hash_table_contains(tb, id)) {
            return g_hash_table_lookup(tb, id);
        }
    } else {
        syslog_server(LOG_ERR, "bahn data util get object - unknown config type");
    }
    return NULL;
}

int interlocking_table_get_routes(const char *src_signal_id, const char *dst_signal_id, char *route_ids[]) {
    if (src_signal_id == NULL || dst_signal_id == NULL) {
        syslog_server(LOG_ERR, "interlocking table get routes: called with invalid (NULL) parameters");
        return 0;
    }
    const GArray *arr = interlocking_table_get_route_ids(src_signal_id, dst_signal_id);
    if (arr != NULL) {
        for (int i = 0; i < arr->len; ++i) {
            route_ids[i] = g_array_index(arr, char *, i);
        }
        return arr->len;
    }
    return 0;
}

int get_route_array_string_value(t_interlocking_route *route, const char *prop_name, char* data[]) {
    if (route == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get route array string value: called with invalid (NULL) parameters");
        return 0;
    }
    if (string_equals(prop_name, "path")) {
        if (route->path != NULL) {
            for (int i = 0; i < route->path->len; ++i) {
                data[i] = g_array_index(route->path, char *, i);
            }
            return route->path->len;
        }
    } else if (string_equals(prop_name, "sections")) {
        if (route->sections != NULL) {
            for (int i = 0; i < route->sections->len; ++i) {
                data[i] = g_array_index(route->sections, char *, i);
            }
            return route->sections->len;
        }
    } else if (string_equals(prop_name, "route_points")) {
        if (route->points != NULL) {
            for (int i = 0; i < route->points->len; ++i) {
                data[i] = (&g_array_index(route->points, t_interlocking_point, i))->id;
            }
            return route->points->len;
        }
    } else if (string_equals(prop_name, "route_signals")) {
        if (route->signals != NULL) {
            for (int i = 0; i < route->signals->len; ++i) {
                data[i] = g_array_index(route->signals, char *, i);
            }
            return route->signals->len;
        }
    } else if (string_equals(prop_name, "conflicts")) {
        if (route->conflicts != NULL) {
            for (int i = 0; i < route->conflicts->len; ++i) {
                data[i] = g_array_index(route->conflicts, char *, i);
            }
            return route->conflicts->len;
        }
    }

    return 0;
}

char *config_get_scalar_string_value(const char *type, const char *id, const char *prop_name) {
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get scalar string: called with invalid (NULL) parameters");
        return "";
    }
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    char *result = NULL;
    if (obj != NULL) {
        switch (config_type) {
            case TYPE_ROUTE:
                if (string_equals(prop_name, "id")) {
                    result = ((t_interlocking_route *) obj)->id;
                } else if (string_equals(prop_name, "source")) {
                    result = ((t_interlocking_route *) obj)->source;
                } else if (string_equals(prop_name, "destination")) {
                    result = ((t_interlocking_route *) obj)->destination;
                } else if (string_equals(prop_name, "orientation")) {
                    result = ((t_interlocking_route *) obj)->orientation;
                } else if (string_equals(prop_name, "train")) {
                    result = ((t_interlocking_route *) obj)->train;
                }
                break;
                
            case TYPE_SEGMENT:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_segment *) obj)->id;
                }
                break;
                
            case TYPE_REVERSER:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_reverser *) obj)->id;
                } else if (string_equals(prop_name, "board")) {
                    result = ((t_config_reverser *) obj)->board;
                } else if (string_equals(prop_name, "block")) {
                    result = ((t_config_reverser *) obj)->block;
                }
                break;
            
            case TYPE_SIGNAL:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_signal *) obj)->id;
                } else if (string_equals(prop_name, "initial")) {
                    result = ((t_config_signal *) obj)->initial;
                } else if (string_equals(prop_name, "type")) {
                    result = ((t_config_signal *) obj)->type;
                }
                break;
                
            case TYPE_POINT:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_point *) obj)->id;
                } else if (string_equals(prop_name, "initial")) {
                    result = ((t_config_point *) obj)->initial;
                } else if (string_equals(prop_name, "segment")) {
                    result = ((t_config_point *) obj)->segment;
                } else if (string_equals(prop_name, "normal")) {
                    result = ((t_config_point *) obj)->normal_aspect;
                } else if (string_equals(prop_name, "reverse")) {
                    result = ((t_config_point *) obj)->reverse_aspect;
                }
                break;
                
            case TYPE_PERIPHERAL:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_peripheral *) obj)->id;
                } else if (string_equals(prop_name, "initial")) {
                    result = ((t_config_peripheral *) obj)->initial;
                } else if (string_equals(prop_name, "type")) {
                    result = ((t_config_peripheral *) obj)->type;
                }
                break;
                
            case TYPE_TRAIN:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_train *) obj)->id;
                } else if (string_equals(prop_name, "type")) {
                    result = ((t_config_train *) obj)->type;
                }
                break;
                
            case TYPE_BLOCK:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_block *) obj)->id;
                } else if (string_equals(prop_name, "direction")) {
                    result = ((t_config_block *) obj)->direction;
                }
                break;
                
            case TYPE_CROSSING:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_crossing *) obj)->id;
                } else if (string_equals(prop_name, "segment")) {
                    result = ((t_config_crossing *) obj)->main_segment;
                }
                break;
                
            case TYPE_SIGNAL_TYPE:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_signal_type *) obj)->id;
                } else if (string_equals(prop_name, "initial")) {
                    result = ((t_config_signal_type *) obj)->initial;
                }
                break;

            case TYPE_COMPOSITE_SIGNAL:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_composite_signal *) obj)->id;
                } else if (string_equals(prop_name, "entry")) {
                    result = ((t_config_composite_signal *) obj)->entry;
                } else if (string_equals(prop_name, "exit")) {
                    result = ((t_config_composite_signal *) obj)->exit;
                } else if (string_equals(prop_name, "block")) {
                    result = ((t_config_composite_signal *) obj)->block;
                } else if (string_equals(prop_name, "distant")) {
                    result = ((t_config_composite_signal *) obj)->distant;
                }
                break;
                
            case TYPE_PERIPHERAL_TYPE:
                if (string_equals(prop_name, "id")) {
                    result = ((t_config_peripheral_type *) obj)->id;
                } else if (string_equals(prop_name, "initial")) {
                    result = ((t_config_peripheral_type *) obj)->initial;
                }
                break;
                
            default:
                break;
        }
    }

    result = result != NULL ? result : "";
    syslog_server(LOG_DEBUG, "Get scalar string: %s %s.%s => \"%s\"", type, id, prop_name, result);
    return result;
}

float config_get_scalar_float_value(const char *type, const char *id, const char *prop_name) {
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get scalar float: called with invalid (NULL) parameters");
        return 0;
    }
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
                } else if (string_equals(prop_name, "limit")) {
                    result = ((t_config_block *) obj)->limit_speed;
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
                } else if (string_equals(prop_name, "weight")) {
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
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get scalar bool: called with invalid (NULL) parameters");
        return "";
    }
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    bool result = false;
    if (obj != NULL) {
        switch (config_type) {
            case TYPE_BLOCK:
                if (string_equals(prop_name, "is_reversed")) {
                    result = ((t_config_block *) obj)->is_reversed;
                }
                break;

            default:
                break;
        }
    }
    
    return result;
}

int config_get_array_string_value(const char *type, const char *id, const char *prop_name, char *data[]) {
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get array string: called with invalid (NULL) parameters");
        return 0;
    }
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
                }
                break;
                
            case TYPE_PERIPHERAL:
                if (string_equals(prop_name, "aspects")) {
                    arr = ((t_config_peripheral *) obj)->aspects;
                }
                break;
                
            case TYPE_TRAIN:
                if (string_equals(prop_name, "peripherals")) {
                    arr = ((t_config_train *) obj)->peripherals;
                }
                break;
                
            case TYPE_BLOCK:
                if (string_equals(prop_name, "train_types")) {
                    arr = ((t_config_block *) obj)->train_types;
                } else if (string_equals(prop_name, "block_signals")) {
                    arr = ((t_config_block *) obj)->signals;
                } else if (string_equals(prop_name, "main_segments")) {
                    arr = ((t_config_block *) obj)->main_segments;
                } else if (string_equals(prop_name, "overlaps")) {
                    arr = ((t_config_block *) obj)->overlaps;
                }
                break;
                
            case TYPE_SIGNAL_TYPE:
                if (string_equals(prop_name, "aspects")) {
                    arr = ((t_config_signal_type *) obj)->aspects;
                }
                break;
                
            case TYPE_PERIPHERAL_TYPE:
                if (string_equals(prop_name, "aspects")) {
                    arr = ((t_config_peripheral_type *) obj)->aspects;
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

int config_get_array_int_value(const char *type, const char *id, const char *prop_name, int data[]) {
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Get array int value: called with invalid (NULL) parameters");
        return 0;
    }
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    int result = 0;
    if (obj != NULL) {
        GArray *arr = NULL;
        switch (config_type) {
            case TYPE_TRAIN:
                if (string_equals(prop_name, "calibration")) {
                    arr = ((t_config_train *) obj)->calibration;
                }
                break;
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
    if (type == NULL || id == NULL || prop_name == NULL) {
        syslog_server(LOG_ERR, "Config set scalar string value: called with invalid (NULL) parameters");
        return false;
    }
    e_config_type config_type = get_config_type(type);
    void *obj = get_object(config_type, id);
    bool result = false;
    if (obj != NULL && config_type == TYPE_ROUTE) {
        if (string_equals(prop_name, "train")) {
            // Set train
            t_interlocking_route *route = (t_interlocking_route *) obj;
            if (route->train != NULL) {
                syslog_server(LOG_ERR, 
                              "config set scalar string value: not allowed to "
                              "overwrite route->train for route-id %s", 
                              route->id);
            } else {
                route->train = strdup(value);
                if (value != NULL && route->train == NULL) {
                    syslog_server(LOG_ERR, 
                                  "config set scalar string value: "
                                  "unable to allocate memory for route->train");
                }
                result = true;
            }
        }
    }

    syslog_server(LOG_DEBUG, 
                  "Set scalar string: %s %s.%s = %s => %s", 
                  type, id, prop_name, value, result ? "true" : "false");
    return result;
}

e_config_type get_track_state_type(const char *id) {
    if (id == NULL) {
        syslog_server(LOG_ERR, "Get track state type: %s is NULL", id);
        return TYPE_NOT_SUPPORTED;
    }
    
    if (g_hash_table_contains(config_data.table_signals, id)) {
        return TYPE_SIGNAL;
    } else if (g_hash_table_contains(config_data.table_points, id)) {
        return TYPE_POINT;
    } else if (g_hash_table_contains(config_data.table_peripherals, id)) {
        return TYPE_PERIPHERAL;
    }

    syslog_server(LOG_ERR, "Get track state type: %s could not be found", id);
    return TYPE_NOT_SUPPORTED;
}

/**
 * Get raw signal aspect from bidib state (stop, go, caution, shunt)
 * Convert back to signalling action based on signal type
 * 
 * @param id signal name
 * @param value stop, go, caution, or shunt
 * @return true of success, otherwise false
 */
static char *get_signal_state(const char *id) {
    if (id == NULL) {
        return "";
    }
    const t_config_signal *signal = get_object(TYPE_SIGNAL, id);
    if (signal == NULL) {
        return "";
    }
    
    // load raw state
    char *raw_state = NULL;
    t_bidib_unified_accessory_state_query state_query = bidib_get_signal_state(id);
    if (state_query.known) {
        if (state_query.board_accessory_state.state_id == NULL) {
            syslog_server(LOG_ERR, "Get signal state: board accessory state id is NULL");
            bidib_free_unified_accessory_state_query(state_query);
            return NULL;
        }
        raw_state = strdup(state_query.board_accessory_state.state_id);
        if (raw_state == NULL) {
            syslog_server(LOG_ERR, "Get signal state: unable to allocate memory for raw_state");
            bidib_free_unified_accessory_state_query(state_query);
            return NULL;
        }
    }
    bidib_free_unified_accessory_state_query(state_query);

    // load signalling aspect based on signal type
    char *result = NULL;
    
    if (string_equals(raw_state, "aspect_stop")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "block")
            || string_equals(signal->type, "distant")
            || string_equals(signal->type, "shunting")
            || string_equals(signal->type, "halt")) {

            result = "stop";
        }
    } else if (string_equals(raw_state, "aspect_go")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "block")
            || string_equals(signal->type, "distant")) {

            result = "go";
        }
    } else if (string_equals(raw_state, "aspect_caution")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "distant")) {

            result = "caution";
        }
    } else if (string_equals(raw_state, "aspect_shunt")) {
        if (string_equals(signal->type, "exit")
            || string_equals(signal->type, "shunting")) {
            
            result = "shunt";
        }
    }
    free(raw_state);
    
    return result;
}

static bool set_signal_raw_aspect(const t_config_signal *signal, const char *value) {
    if (signal == NULL || value == NULL) {
        syslog_server(LOG_ERR, "Set signal raw aspect: invalid (NULL) parameters");
        return false;
    }
    if (signal->aspects != NULL) {
        for (int i = 0; i < signal->aspects->len; ++i) {
            char *aspect = g_array_index(signal->aspects, char *, i);
            if (string_equals(aspect, value)) {
                bool result = bidib_set_signal(signal->id, value) == 0;
                bidib_flush();
                return result;
            }
        }
    }
    return false;
}

/**
 * Convert the signalling action to raw aspect based on signal types
 * Update bidib state
 * 
 * @param id signal name
 * @param value stop, go, caution, shunt
 * @return true if successful, otherwise false
 */
static bool set_signal_state(const char *id, const char *value) {
    if (id == NULL || value == NULL) {
        syslog_server(LOG_ERR, "Set signal state: invalid (NULL) parameters");
        return false;
    }
    const t_config_signal *signal = get_object(TYPE_SIGNAL, id);
    if (signal == NULL) {
        return false;
    }
    
    if (string_equals(value, "stop")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "distant")
            || string_equals(signal->type, "block")
            || string_equals(signal->type, "shunting")
            || string_equals(signal->type, "halt")) {

            return set_signal_raw_aspect(signal, "aspect_stop");
        }
    } else if (string_equals(value, "go")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "distant")
            || string_equals(signal->type, "block")) {

            return set_signal_raw_aspect(signal, "aspect_go");
        }
    } else if (string_equals(value, "caution")) {
        if (string_equals(signal->type, "entry")
            || string_equals(signal->type, "exit")
            || string_equals(signal->type, "distant")) {

            return set_signal_raw_aspect(signal, "aspect_caution");
        }
    } else if (string_equals(value, "shunt")) {
        if (string_equals(signal->type, "exit")
            || string_equals(signal->type, "shunting")) {

            return set_signal_raw_aspect(signal, "aspect_shunt");
        }
    }

    return false;
}

/**
 * Set the peripheral aspect to some value
 * 
 * @param peripheral peripheral whose aspect to set
 * @param value value of the aspect
 * @return true valid params
 * @return false invalid params
 */
static bool set_peripheral_raw_aspect(t_config_peripheral *peripheral, const char *value) {
    if (peripheral == NULL || value == NULL) {
        syslog_server(LOG_ERR, "Set peripheral raw aspect: invalid (NULL) parameters");
        return false;
    }
    if (peripheral->aspects != NULL) {
        for (int i = 0; i < peripheral->aspects->len; ++i) {
            char *aspect = g_array_index(peripheral->aspects, char *, i);
            if (string_equals(aspect, value)) {
                bool result = bidib_set_peripheral(peripheral->id, value) == 0;
                bidib_flush();
                return result;
            }
        }
    }

    return false;
}

/**
 * Convert the peripheral action to raw aspect based on peripheral types
 * Update bidib state
 * 
 * @param id peripheral name
 * @param value on or off
 * @return true if successful, otherwise false
 */
static bool set_peripheral_state(const char *id, const char *value) {
    if (id == NULL || value == NULL) {
        syslog_server(LOG_ERR, "Set peripheral state: invalid (NULL) parameters");
        return false;
    }
    t_config_peripheral *peripheral = get_object(TYPE_PERIPHERAL, id);
    if (peripheral == NULL) {
        return false;
    }
    
    if (string_equals(peripheral->type, "onebit")) {
        if (string_equals(value, "on")) {
            return set_peripheral_raw_aspect(peripheral, "high");
        } else if (string_equals(value, "off")) {
            return set_peripheral_raw_aspect(peripheral, "low");
        }
    }
    return false;
}

char *track_state_get_value(const char *id) {
    if (id == NULL) {
        syslog_server(LOG_ERR, "Track state get value: called with invalid (NULL) id parameter");
        return "";
    }
    char *result = NULL;
    e_config_type config_type = get_track_state_type(id);
    void *obj = get_object(config_type, id);
    if (obj != NULL) {
        if (config_type == TYPE_POINT) {
            t_bidib_unified_accessory_state_query state_query = bidib_get_point_state(id);
            if (state_query.known) {
                result = strdup(state_query.board_accessory_state.state_id);
                if (state_query.board_accessory_state.state_id != NULL && result == NULL) {
                    syslog_server(LOG_ERR, 
                                  "track state get value: unable to allocate "
                                  "memory for result for TYPE_POINT");
                }
            }
            bidib_free_unified_accessory_state_query(state_query);
        } else if (config_type == TYPE_SIGNAL) {
            result = strdup(get_signal_state(id));
        } else if (config_type == TYPE_PERIPHERAL) {
            t_bidib_peripheral_state_query state_query = bidib_get_peripheral_state(id);
            if (state_query.available) {
                result = strdup(state_query.data.state_id);
                if (state_query.data.state_id != NULL && result == NULL) {
                    syslog_server(LOG_ERR, 
                                  "track state get value: unable to allocate "
                                  "memory for result for TYPE_PERIPHERAL");
                }
            }
            bidib_free_peripheral_state_query(state_query);
        }
    }

    if (result != NULL) {
        // Add to cache such that the memory can be freed later via 
        // bahn_data_util_free_cached_track_state
        add_cache_str(result);
    } else {
        result = "";
    }

    syslog_server(LOG_DEBUG, "Get track state: %s => %s", id, result);
    return result;
}

bool track_state_set_value(const char *id, const char *value) {
    if (id == NULL || value == NULL) {
        syslog_server(LOG_ERR, "Track state set value: invalid (NULL) parameters");
        return false;
    }
    e_config_type config_type = get_track_state_type(id);
    bool result = false;
    switch (config_type) {
        case TYPE_POINT:
            if (string_equals(value, "normal") || string_equals(value, "reverse")) {
                result = bidib_switch_point(id, value) == 0;
                bidib_flush();
                syslog_server(LOG_DEBUG, 
                              "Set point state: %s to %s => %s", 
                              id, value, result ? "true" : "false");
            } else {
                syslog_server(LOG_DEBUG, "Invalid point state: %s", value);
            }
            return result;
        case TYPE_SIGNAL:
            result = set_signal_state(id, value);
            bidib_flush();
            syslog_server(LOG_DEBUG, 
                          "Set signal state: %s to %s => %s",
                          id, value, result ? "true" : "false");
            return result;
        case TYPE_PERIPHERAL:
            result = set_peripheral_state(id, value);
            bidib_flush();
            syslog_server(LOG_DEBUG, 
                          "Set peripheral state: %s to %s => %s",
                          id, value, result ? "true" : "false");
            return result;
        default:
            return false;
    }
}

bool is_segment_occupied(const char *id) {
    if (id == NULL) {
        return false;
    }
    bool result = false;
    if (g_hash_table_contains(config_data.table_segments, id)) {
        t_bidib_segment_state_query state_query = bidib_get_segment_state(id);
        result = state_query.known && state_query.data.occupied;
        bidib_free_segment_state_query(state_query);
    } else {
        syslog_server(LOG_WARNING, "Is segment occupied: unknown segment %s", id);
    }

    syslog_server(LOG_DEBUG, "Is segment occupied: %s => %s", id, result ? "true" : "false");
    return result;
}

bool is_type_segment(const char *id) {
    if (id == NULL) {
        return false;
    }
    bool result = g_hash_table_contains(config_data.table_segments, id);

    syslog_server(LOG_DEBUG, "Is %s a segment: %s", id, result ? "true" : "false");
    return result;
}

bool is_type_signal(const char *id) {
    if (id == NULL) {
        return false;
    }
    bool result = g_hash_table_contains(config_data.table_signals, id);

    syslog_server(LOG_DEBUG, "Is %s a signal: %s", id, result ? "true" : "false");
    return result;
}

int train_state_get_speed(const char *train_id) {
    if (train_id == NULL) {
        syslog_server(LOG_ERR, "Train state get speed (km/h): invalid (NULL) train_id parameter");
        return 0;
    }
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
    if (train_id == NULL) {
        syslog_server(LOG_ERR, "Train state set speed: invalid (NULL) train_id parameter");
        return false;
    }
    bool result = false;
    if (g_hash_table_contains(config_data.table_trains, train_id)) {
        const int grab_id = train_get_grab_id(train_id);
        pthread_mutex_lock(&grabbed_trains_mutex);
        result = bidib_set_train_speed(train_id, speed, grabbed_trains[grab_id].track_output) == 0;
        pthread_mutex_unlock(&grabbed_trains_mutex);
        bidib_flush();
    } else {
        syslog_server(LOG_ERR, "Invalid train id: %s", train_id);
    }

    syslog_server(LOG_DEBUG,  
                  "Set train speed in km/h: %s to %d => %s", 
                  train_id, speed, result ? "true" : "false");
    return result;
}

char *config_get_point_position(const char *route_id, const char *point_id) {
    if (route_id == NULL || point_id == NULL) {
        syslog_server(LOG_ERR, "Get route point position: invalid (NULL) parameters");
        return "";
    }
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

    result = result != NULL ? result : "";
    syslog_server(LOG_DEBUG, "Get route point position: %s.%s => %s", route_id, point_id, result);
    return result;
}

char *config_get_block_id_of_segment(const char *seg_id) {
    if (seg_id == NULL) {
        syslog_server(LOG_ERR, "Get block id of segment: invalid (NULL) seg_id parameter");
        return "";
    }
    GHashTableIter iterator;
    g_hash_table_iter_init(&iterator, config_data.table_blocks);
    
    gpointer key;
    gpointer value;
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        char *block_id = (char *)key;
        const t_config_block *block_details = (t_config_block *)value;
        
        // A block always has a main segment
        const GArray *main_segments = block_details->main_segments;
        for (int i = 0; i < main_segments->len; ++i) {
            const char *main_segment = g_array_index(main_segments, const char *, i);
            if (strcmp(seg_id, main_segment) == 0) {
                return block_id;
            }
        }        
        // A block may have no overlap segments
        const GArray *overlaps = block_details->overlaps;
        if (overlaps == NULL) {
            continue;
        }
        for (int i = 0; i < overlaps->len; ++i) {
            const char *overlap = g_array_index(overlaps, const char *, i);
            if (strcmp(seg_id, overlap) == 0) {
                return block_id;
            }
        }
    }
    return "";
}

void log_bool(bool value) {
    syslog_server(LOG_INFO, "%s", value ? "true" : "false");
}

void log_int(int value) {
    syslog_server(LOG_INFO, "%d", value);
}

void log_float(float value) {
    syslog_server(LOG_INFO, "%f", value);
}

void log_string(const char *value) {
    syslog_server(LOG_INFO, "%s", value);
}
