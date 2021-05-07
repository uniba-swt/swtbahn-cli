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
    POINT_ASPECT
} e_track_mapping_level;

typedef enum {
    TRACK_SEQ_NONE,
    BOARDS,
    SEGMENTS,
    SIGNALS,
    SIGNAL_ASPECTS,
    POINTS,
    POINT_ASPECTS
} e_track_sequence_level;

GHashTable *tb_segments;
GHashTable *tb_signals;
GHashTable *tb_points;
t_config_segment *cur_segment;
t_config_signal *cur_signal;
t_config_point *cur_point;
e_track_mapping_level track_mapping = TRACK_ROOT;
e_track_sequence_level track_sequence = TRACK_SEQ_NONE;

void free_track_id_key(void *pointer) {
    log_debug("free key: %s", (char *) pointer);
    free(pointer);
}

void free_segment(void *pointer) {
    t_config_segment *segment = (t_config_segment *) pointer;
    log_debug("free segment: %s", segment->id);
    free(segment);
}

void free_signal(void *pointer) {
    t_config_signal *signal = (t_config_signal *) pointer;
    log_debug("free signal: %s", signal->id);

    if (signal->aspects != NULL) {
        log_debug("\tfree aspects:");
        for (int i = 0; i < signal->aspects->len; ++i) {
            log_debug("\t\t%s", g_array_index(signal->aspects, char *, i));
        }
        g_array_free(signal->aspects, true);
    }

    free(signal);
}

void free_point(void *pointer) {
    t_config_point *point = (t_config_point *) pointer;
    log_debug("free point: %s", point->id);
    free(point);
}

void track_yaml_sequence_start(char *scalar) {
    if (track_mapping == TRACK_ROOT && str_equal(scalar, "boards")) {
        track_sequence = BOARDS;
        return;
    }

    if (track_mapping == BOARD && str_equal(scalar, "segments")) {
        track_sequence = SEGMENTS;
        if (tb_segments == NULL)
            tb_segments = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_segment);
        return;
    }

    if (track_mapping == BOARD && str_equal(scalar, "signals-board")) {
        track_sequence = SIGNALS;
        if (tb_signals == NULL)
            tb_signals = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_signal);
        return;
    }

    if (track_mapping == SIGNAL && str_equal(scalar, "aspects")) {
        track_sequence = SIGNAL_ASPECTS;
        cur_signal->aspects = g_array_sized_new(false, false, sizeof(char *), 4);
        return;
    }

    if (track_mapping == BOARD && str_equal(scalar, "points-board")) {
        track_sequence = POINTS;
        if (tb_points == NULL)
            tb_points = g_hash_table_new_full(g_str_hash, g_str_equal, free_track_id_key, free_point);
        return;
    }

    if (track_mapping == POINT && str_equal(scalar, "aspects")) {
        track_sequence = POINT_ASPECTS;
        return;
    }
}

void track_yaml_sequence_end(char *scalar) {
    // decrease sequence level
    switch (track_sequence) {
        case SEGMENTS:
        case SIGNALS:
        case POINTS:
            track_sequence = BOARDS;
            break;
        case SIGNAL_ASPECTS:
            track_sequence = SIGNALS;
            break;
        case POINT_ASPECTS:
            track_sequence = POINTS;
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
            cur_segment->id = NULL;
            break;
        case SIGNALS:
            track_mapping = SIGNAL;
            cur_signal = malloc(sizeof(t_config_signal));
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
            cur_point->id = NULL;
            cur_point->initial = NULL;
            cur_point->segment = NULL;
            cur_point->normal_aspect = NULL;
            cur_point->reverse_aspect = NULL;
            break;
        case POINT_ASPECTS:
            track_mapping = POINT_ASPECT;
            break;
        default:
            break;
    }
}

void track_yaml_mapping_end(char *scalar) {
    // insert mapping to hash table
    switch (track_mapping) {
        case SEGMENT:
            log_debug("insert segment: %s", cur_segment->id);
            g_hash_table_insert(tb_segments, strdup(cur_segment->id), cur_segment);
            break;
        case SIGNAL:
            log_debug("insert signal: %s", cur_signal->id);
            g_hash_table_insert(tb_signals, strdup(cur_signal->id), cur_signal);
            break;
        case POINT:
            log_debug("insert point: %s", cur_point->id);
            g_hash_table_insert(tb_points, strdup(cur_point->id), cur_point);
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
            track_mapping = BOARD;
            break;
        case POINT_ASPECT:
            track_mapping = POINT;
            return;
        case SIGNAL_ASPECT:
            track_mapping = SIGNAL;
            return;
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
            }

            if (str_equal(last_scalar, "length")) {
                cur_segment->length = parse_float(cur_scalar);
                return;
            }

        case SIGNAL:
            if (str_equal(last_scalar, "id")) {
                cur_signal->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "initial")) {
                cur_signal->initial = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "type")) {
                cur_signal->type = cur_scalar;
                return;
            }

        case POINT:
            if (str_equal(last_scalar, "id")) {
                cur_point->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "initial")) {
                cur_point->initial = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "segment")) {
                cur_point->segment = cur_scalar;
                return;
            }

        case SIGNAL_ASPECT:
            if (str_equal(last_scalar, "id")) {
                g_array_append_val(cur_signal->aspects, cur_scalar);
            }

        case POINT_ASPECT:
            if (str_equal(last_scalar, "id")) {
                if (str_equal(cur_scalar, "normal")) {
                    cur_point->normal_aspect = cur_scalar;
                    return;
                }

                if (str_equal(cur_scalar, "reverse")) {
                    cur_point->reverse_aspect = cur_scalar;
                    return;
                }
            }

        default:
            return;
    }
}

void parse_track_yaml(yaml_parser_t *parser, t_config_data *data) {
    parse_yaml_content(parser, track_yaml_sequence_start, track_yaml_sequence_end, track_yaml_mapping_start, track_yaml_mapping_end, track_yaml_scalar);
    data->table_segments = tb_segments;
    data->table_signals = tb_signals;
    data->table_points = tb_points;
}