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

#include "train_config_parser.h"
#include "parser_util.h"

typedef enum {
    TRAIN_ROOT,
    TRAIN,
    PERIPHERAL,
} e_train_mapping_level;

typedef enum {
    TRAIN_SEQ_NONE,
    TRAINS,
    PERIPHERALS,
    CALIBRATIONS
} e_train_sequence_level;

GHashTable *tb_trains;
t_config_train *cur_train;
e_train_mapping_level train_mapping = TRAIN_ROOT;
e_train_sequence_level train_sequence = TRAIN_SEQ_NONE;

void free_train_id_key(void *pointer) {
    log_debug("free key: %s", (char *) pointer);
    free(pointer);
}

void free_train(void *pointer) {
    t_config_train *train = (t_config_train *) pointer;
    if (train == NULL) {
        return;
    }
    if (train->id != NULL) {
        log_debug("free train: %s", train->id);
        free(train->id);
        train->id = NULL;
    }
    if (train->type != NULL) {
        free(train->type);
        train->type = NULL;
    }
    if (train->peripherals != NULL) {
        log_debug("\tfree peripherals");
        for (int i = 0; i < train->peripherals->len; ++i) {
            log_debug("\t\t%s", g_array_index(train->peripherals, char *, i));
        }
        g_array_free(train->peripherals, true);
    }
    if (train->calibration != NULL) {
        log_debug("\tfree calibration");
        for (int i = 0; i < train->calibration->len; ++i) {
            log_debug("\t\t%d", g_array_index(train->calibration, int, i));
        }
        g_array_free(train->calibration, true);
    }
    free(train);
}

void nullify_train_config_table(void) {
	tb_trains = NULL;
}

void train_yaml_sequence_start(char *scalar) {	
    log_debug("train_yaml_sequence_start: %s", scalar);
    if (train_mapping == TRAIN_ROOT && str_equal(scalar, "trains")) {
        train_sequence = TRAINS;
        if (tb_trains == NULL) {
            tb_trains = g_hash_table_new_full(g_str_hash, g_str_equal, free_train_id_key, free_train);
        }
        return;
    }

    if (train_mapping == TRAIN && str_equal(scalar, "peripherals")) {
        train_sequence = PERIPHERALS;
        cur_train->peripherals = g_array_sized_new(false, false, sizeof(char *), 4);
        return;
    }

    if (train_mapping == TRAIN && str_equal(scalar, "calibration")) {
        train_sequence = CALIBRATIONS;
        cur_train->calibration = g_array_sized_new(false, false, sizeof(int), 10);
        return;
    }
}

void train_yaml_sequence_end(char *scalar) {
    log_debug("train_yaml_sequence_end: %s", scalar);
    switch (train_sequence) {
        case PERIPHERALS:
        case CALIBRATIONS:
            train_sequence = TRAINS;
            break;
        default:
            break;
    }
}
void train_yaml_mapping_start(char *scalar) {
    log_debug("train_yaml_mapping_start: %s", scalar);
    switch (train_sequence) {
        case TRAINS:
            train_mapping = TRAIN;
            cur_train = malloc(sizeof(t_config_train));
            cur_train->id = NULL;
            cur_train->type = NULL;
            cur_train->peripherals = NULL;
            cur_train->calibration = NULL;
            break;
        case PERIPHERALS:
            train_mapping = PERIPHERAL;
            break;
        default:
            break;
    }
}

void train_yaml_mapping_end(char *scalar) {
    log_debug("train_yaml_mapping_end: %s", scalar);

    // insert mapping to hash table
    if (train_mapping == TRAIN) {
        log_debug("train_yaml_mapping_end: insert train: %s", cur_train->id);
        g_hash_table_insert(tb_trains, strdup(cur_train->id), cur_train);
    }

    // decrease mapping level
    switch (train_mapping) {
        case TRAIN:
            train_mapping = TRAIN_ROOT;
            break;
        case PERIPHERAL:
            train_mapping = TRAIN;
            break;
        default:
            break;
    }
}

void train_yaml_scalar(char *last_scalar, char *cur_scalar) {

    if (train_sequence == CALIBRATIONS) {
        int cal = (int)strtol(cur_scalar, NULL, 10);
        g_array_append_val(cur_train->calibration, cal);
        free(cur_scalar);
        cur_scalar = NULL;
        return;
    }

    switch (train_mapping) {
        case TRAIN:
            if (str_equal(last_scalar, "id")) {
                cur_train->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "length")) {
                cur_train->length = parse_float(cur_scalar);
                break;
            }

            if (str_equal(last_scalar, "weight")) {
                cur_train->weight = parse_float(cur_scalar);
                break;
            }

            if (str_equal(last_scalar, "type")) {
                cur_train->type = cur_scalar;
                return;
            }
            break;
        case PERIPHERAL:
            if (str_equal(last_scalar, "id")) {
                g_array_append_val(cur_train->peripherals, cur_scalar);
                break;
            }
            break;
        default:
            break;
    }
    free(cur_scalar);
    cur_scalar = NULL;
}

void parse_train_yaml(yaml_parser_t *parser, t_config_data *data) {
    train_mapping = TRAIN_ROOT;
    train_sequence = TRAIN_SEQ_NONE;

    parse_yaml_content(parser, train_yaml_sequence_start, train_yaml_sequence_end, train_yaml_mapping_start, train_yaml_mapping_end, train_yaml_scalar);
    data->table_trains = tb_trains;
}
