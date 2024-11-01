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

#include <yaml.h>

#include "track_config_parser.h"
#include "parser_util.h"

typedef enum {
    TRACK_ROOT,
    BOARD,
    SEGMENT,
    SIGNAL,
    SIGNAL_ASPECT,
    POINT,
    POINT_ASPECT,
    PERIPHERAL,
    PERIPHERAL_ASPECT
} e_track_mapping_level;

typedef enum {
    TRACK_SEQ_NONE,
    BOARDS,
    SEGMENTS,
    SIGNALS,
    SIGNAL_ASPECTS,
    POINTS,
    POINT_ASPECTS,
    PERIPHERALS,
    PERIPHERAL_ASPECTS
} e_track_sequence_level;

GHashTable *tb_segments;
GHashTable *tb_signals;
GHashTable *tb_points;
GHashTable *tb_peripherals;
t_config_segment *cur_segment;
t_config_signal *cur_signal;
t_config_point *cur_point;
t_config_peripheral *cur_peripheral;
e_track_mapping_level track_mapping = TRACK_ROOT;
e_track_sequence_level track_sequence = TRACK_SEQ_NONE;

void free_track_id_key(void *pointer) {
    log_debug("free key: %s", (char *) pointer);
    free(pointer);
}

void free_segment(void *pointer) {
    t_config_segment *segment = (t_config_segment *) pointer;
    if (segment == NULL) {
        return;
    }
    if (segment->id != NULL) {
        log_debug("free segment: %s", segment->id);
        free(segment->id);
        segment->id = NULL;
    }
    free(segment);
}

void free_signal(void *pointer) {
    t_config_signal *signal = (t_config_signal *) pointer;
    if (signal == NULL) {
        return;
    }
    if (signal->id != NULL) {
        log_debug("free signal: %s", signal->id);
        free(signal->id);
        signal->id = NULL;
    }
    if (signal->aspects != NULL) {
        log_debug("\tfree signal aspects");
        for (int i = 0; i < signal->aspects->len; ++i) {
            free(g_array_index(signal->aspects, char *, i));
        }
        g_array_free(signal->aspects, true);
    }
    if (signal->initial != NULL) {
        log_debug("\tfree signal initial");
        free(signal->initial);
        signal->initial = NULL;
    }
    if (signal->type != NULL) {
        log_debug("\tfree signal type");
        free(signal->type);
        signal->type = NULL;
    }
    
    free(signal);
}

void free_point(void *pointer) {
    t_config_point *point = (t_config_point *) pointer;
    if (point == NULL) {
        return;
    }
    if (point->id != NULL) {
        log_debug("free point: %s", point->id);
        free(point->id);
        point->id = NULL;
    }
    if (point->initial != NULL) {
        log_debug("\tfree point initial");
        free(point->initial);
        point->initial = NULL;
    }
    if (point->normal_aspect != NULL) {
        log_debug("\tfree point normal aspect");
        free(point->normal_aspect);
        point->normal_aspect = NULL;
    }
    if (point->reverse_aspect != NULL) {
        log_debug("\tfree point reverse aspect");
        free(point->reverse_aspect);
        point->reverse_aspect = NULL;
    }
    if (point->segment != NULL) {
        log_debug("\tfree point segment");
        free(point->segment);
        point->segment = NULL;
    }
    free(point);
}

void free_peripheral(void *pointer) {
    t_config_peripheral *peripheral = (t_config_peripheral *) pointer;
    if (peripheral == NULL) {
        return;
    }
    if (peripheral->id != NULL) {
        log_debug("free peripheral: %s", peripheral->id);
        free(peripheral->id);
        peripheral->id = NULL;
    }

    if (peripheral->aspects != NULL) {
        log_debug("\tfree peripheral aspects");
        for (int i = 0; i < peripheral->aspects->len; ++i) {
            free(g_array_index(peripheral->aspects, char *, i));
        }
        g_array_free(peripheral->aspects, true);
    }
    if (peripheral->initial != NULL) {
        log_debug("\tfree peripheral initial");
        free(peripheral->initial);
        peripheral->initial = NULL;
    }
    if (peripheral->type != NULL) {
        log_debug("\tfree peripheral type");
        free(peripheral->type);
        peripheral->type = NULL;
    }
    free(peripheral);
}

void nullify_track_config_tables(void) {
    tb_segments = NULL;
    tb_signals = NULL;
    tb_points = NULL;
    tb_peripherals = NULL;
}

void initialise_hashtables(void) {
    if (tb_segments == NULL) {
        tb_segments = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_segment);
    }
    if (tb_signals == NULL) {
        tb_signals = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_signal);
    }
    if (tb_points == NULL) {
        tb_points = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_point);
    }
    if (tb_peripherals == NULL) {
        tb_peripherals = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_peripheral);
    }
}

void track_yaml_sequence_start(char *scalar) {
    if (track_mapping == TRACK_ROOT && str_equal(scalar, "boards")) {
        track_sequence = BOARDS;
    } else if (track_mapping == BOARD && str_equal(scalar, "segments")) {
        track_sequence = SEGMENTS;
    } else if (track_mapping == BOARD && str_equal(scalar, "signals-board")) {
        track_sequence = SIGNALS;
    } else if (track_mapping == SIGNAL && str_equal(scalar, "aspects")) {
        track_sequence = SIGNAL_ASPECTS;
        cur_signal->aspects = g_array_sized_new(false, false, sizeof(char *), 4);
    } else if (track_mapping == BOARD && str_equal(scalar, "points-board")) {
        track_sequence = POINTS;
    } else if (track_mapping == POINT && str_equal(scalar, "aspects")) {
        track_sequence = POINT_ASPECTS;
    } else if (track_mapping == BOARD && str_equal(scalar, "peripherals")) {
        track_sequence = PERIPHERALS;
    } else if (track_mapping == PERIPHERAL && str_equal(scalar, "aspects")) {
        track_sequence = PERIPHERAL_ASPECTS;
        cur_peripheral->aspects = g_array_sized_new(false, false, sizeof(char *), 4);
    }
}

void track_yaml_sequence_end(char *scalar) {
    // decrease sequence level
    switch (track_sequence) {
        case SEGMENTS:
        case SIGNALS:
        case POINTS:
        case PERIPHERALS: 
            track_sequence = BOARDS;
            break;
        case SIGNAL_ASPECTS:
            track_sequence = SIGNALS;
            break;
        case POINT_ASPECTS:
            track_sequence = POINTS;
            break;
        case PERIPHERAL_ASPECTS:
            track_sequence = PERIPHERALS;
            break;
        default:
            break;
    }
}

void track_yaml_mapping_start(char *scalar) {
    switch (track_sequence) {
        case BOARDS:
            track_mapping = BOARD;
            break;
        case SEGMENTS:
            track_mapping = SEGMENT;
            cur_segment = malloc(sizeof(t_config_segment));
            if (cur_segment == NULL) {
                log_debug("track_yaml_mapping_start: failed to allocate memory for cur_segment");
                exit(1);
            }
            cur_segment->id = NULL;
            break;
        case SIGNALS:
            track_mapping = SIGNAL;
            cur_signal = malloc(sizeof(t_config_signal));
            if (cur_signal == NULL) {
                log_debug("track_yaml_mapping_start: failed to allocate memory for cur_signal");
                exit(1);
            }
            cur_signal->id = NULL;
            cur_signal->initial = NULL;
            cur_signal->aspects = NULL;
            cur_signal->type = NULL;
            break;
        case SIGNAL_ASPECTS:
            track_mapping = SIGNAL_ASPECT;
            break;
        case POINTS:
            track_mapping = POINT;
            cur_point = malloc(sizeof(t_config_point));
            if (cur_point == NULL) {
                log_debug("track_yaml_mapping_start: failed to allocate memory for cur_point");
                exit(1);
            }
            cur_point->id = NULL;
            cur_point->initial = NULL;
            cur_point->segment = NULL;
            cur_point->normal_aspect = NULL;
            cur_point->reverse_aspect = NULL;
            break;
        case POINT_ASPECTS:
            track_mapping = POINT_ASPECT;
            break;
        case PERIPHERALS:
            track_mapping = PERIPHERAL;
            cur_peripheral = malloc(sizeof(t_config_peripheral));
            if (cur_peripheral == NULL) {
                log_debug("track_yaml_mapping_start: failed to allocate memory for cur_peripheral");
                exit(1);
            }
            cur_peripheral->id = NULL;
            cur_peripheral->initial = NULL;
            cur_peripheral->aspects = NULL;
            cur_peripheral->type = NULL;
            break;
        case PERIPHERAL_ASPECTS:
            track_mapping = PERIPHERAL_ASPECT;
            break;

        default:
            break;
    }
}

void track_yaml_mapping_end(char *scalar) {
    // insert mapping to hash table
    switch (track_mapping) {
        case SEGMENT:
            log_debug("track_yaml_mapping_end: insert segment: %s", cur_segment->id);
            g_hash_table_insert(tb_segments, strdup(cur_segment->id), cur_segment);
            break;
        case SIGNAL:
            log_debug("track_yaml_mapping_end: insert signal: %s", cur_signal->id);
            g_hash_table_insert(tb_signals, strdup(cur_signal->id), cur_signal);
            break;
        case POINT:
            log_debug("track_yaml_mapping_end: insert point: %s", cur_point->id);
            g_hash_table_insert(tb_points, strdup(cur_point->id), cur_point);
            break;
        case PERIPHERAL:
            log_debug("track_yaml_mapping_end: insert peripheral: %s", cur_peripheral->id);
            g_hash_table_insert(tb_peripherals, strdup(cur_peripheral->id), cur_peripheral);
            break;
        default:
            break;
    }

    // decrease mapping level
    switch (track_mapping) {
        case BOARD:
            track_mapping = TRACK_ROOT;
            break;
        case SEGMENT:
        case SIGNAL:
        case POINT:
        case PERIPHERAL:
            track_mapping = BOARD;
            break;
        case POINT_ASPECT:
            track_mapping = POINT;
            break;
        case SIGNAL_ASPECT:
            track_mapping = SIGNAL;
            break;
        case PERIPHERAL_ASPECT:
            track_mapping = PERIPHERAL;
            break;
        default:
            break;
    }
}

void track_yaml_scalar(char *last_scalar, char *cur_scalar) {
    switch (track_mapping) {
        case SEGMENT:
            if (str_equal(last_scalar, "id")) {
                cur_segment->id = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "length")) {
                cur_segment->length = parse_float(cur_scalar);
                // no return here intentional, have to free cur_scalar since no ownership is 
                // transferred here.
            }
            break;

        case SIGNAL:
            if (str_equal(last_scalar, "id")) {
                cur_signal->id = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "initial")) {
                cur_signal->initial = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "type")) {
                cur_signal->type = cur_scalar;
                return;
            }
            break;

        case POINT:
            if (str_equal(last_scalar, "id")) {
                cur_point->id = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "initial")) {
                cur_point->initial = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "segment")) {
                cur_point->segment = cur_scalar;
                return;
            }
            break;

        case PERIPHERAL:
            if (str_equal(last_scalar, "id")) {
                cur_peripheral->id = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "initial")) {
                cur_peripheral->initial = cur_scalar;
                return;
            } else if (str_equal(last_scalar, "type")) {
                cur_peripheral->type = cur_scalar;
                return;
            }
            break;

        case SIGNAL_ASPECT:
            if (str_equal(last_scalar, "id")) {
                g_array_append_val(cur_signal->aspects, cur_scalar);
                return;
            }
            break;

        case POINT_ASPECT:
            if (str_equal(last_scalar, "id")) {
                if (str_equal(cur_scalar, "normal")) {
                    cur_point->normal_aspect = cur_scalar;
                    return;
                } else if (str_equal(cur_scalar, "reverse")) {
                    cur_point->reverse_aspect = cur_scalar;
                    return;
                }
            }
            break;

        case PERIPHERAL_ASPECT:
            if (str_equal(last_scalar, "id")) {
                g_array_append_val(cur_peripheral->aspects, cur_scalar);
                return;
            }
            break;
            
        default:
            break;
    }
    free(cur_scalar);
    cur_scalar = NULL;
}

void parse_track_yaml(yaml_parser_t *parser, t_config_data *data) {
    track_mapping = TRACK_ROOT;
    track_sequence = TRACK_SEQ_NONE;
    
    initialise_hashtables();
    parse_yaml_content(parser, track_yaml_sequence_start, track_yaml_sequence_end, track_yaml_mapping_start, track_yaml_mapping_end, track_yaml_scalar);
    data->table_segments = tb_segments;
    data->table_signals = tb_signals;
    data->table_points = tb_points;
    data->table_peripherals = tb_peripherals;
}
