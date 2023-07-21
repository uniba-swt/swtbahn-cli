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

#include "extras_config_parser.h"
#include "parser_util.h"

typedef enum {
    EXTRAS_ROOT,
    BLOCK,
    REVERSER,
    CROSSING,
    SIGNAL_TYPE,
    COMPOSITE_SIGNAL,
    PERIPHERAL_TYPE
} e_extras_mapping_level;

typedef enum {
    EXTRAS_SEQ_NONE,
    BLOCKS,
    BLOCK_MAIN_SEGMENTS,
    BLOCK_SIGNALS,
    BLOCK_OVERLAPS,
    BLOCK_TRAIN_TYPES,
    REVERSERS,
    CROSSINGS,
    COMPOSITE_SIGNALS,
    SIGNAL_TYPES,
    SIGNAL_TYPE_INITIAL,
    SIGNAL_TYPE_ASPECTS,
    PERIPHERAL_TYPES,
    PERIPHERAL_TYPE_INITIAL,
    PERIPHERAL_TYPE_ASPECTS
} e_extras_sequence_level;

GHashTable *tb_blocks;
GHashTable *tb_reversers;
GHashTable *tb_crossings;
GHashTable *tb_signal_types;
GHashTable *tb_composite_signals;
GHashTable *tb_peripheral_types;

t_config_block *cur_block;
t_config_reverser *cur_reverser;
t_config_crossing *cur_crossing;
t_config_signal_type *cur_signal_type;
t_config_composite_signal *cur_composite_signal;
t_config_peripheral_type *cur_peripheral_type;

e_extras_mapping_level extras_mapping = EXTRAS_ROOT;
e_extras_sequence_level extras_sequence = EXTRAS_SEQ_NONE;

void free_extras_id_key(void *pointer) {
    log_debug("free key: %s", (char *) pointer);
    free(pointer);
}

void free_block(void *pointer) {
    t_config_block *block = (t_config_block *) pointer;
    if (block == NULL) {
        return;
    }
    if (block->id != NULL) {
        log_debug("free block: %s", block->id);
        free(block->id);
        block->id = NULL;
    }
    if (block->direction != NULL) {
        log_debug("\tfree block direction");
        free(block->direction);
        block->direction = NULL;
    }

    if (block->main_segments != NULL) {
        log_debug("\tfree block main segments");
        for (int i = 0; i < block->main_segments->len; ++i) {
            free(g_array_index(block->main_segments, char *, i));
        }
        g_array_free(block->main_segments, true);
    }

    if (block->overlaps != NULL) {
        log_debug("\tfree block overlaps");
        for (int i = 0; i < block->overlaps->len; ++i) {
            free(g_array_index(block->overlaps, char *, i));
        }
        g_array_free(block->overlaps, true);
    }

    if (block->signals != NULL) {
        log_debug("\tfree block signals");
        for (int i = 0; i < block->signals->len; ++i) {
            free(g_array_index(block->signals, char *, i));
        }
        g_array_free(block->signals, true);
    }

    if (block->train_types != NULL) {
        log_debug("\tfree block train types");
        for (int i = 0; i < block->train_types->len; ++i) {
            free(g_array_index(block->train_types, char *, i));
        }
        g_array_free(block->train_types, true);
    }

    free(block);
}

void free_reverser(void *pointer) {
    t_config_reverser *reverser = (t_config_reverser *) pointer;
    if (reverser == NULL) {
        return;
    }
    if (reverser->id != NULL) {
        log_debug("free reverser: %s", reverser->id);
        free(reverser->id);
        reverser->id = NULL;
    }
    if (reverser->board != NULL) {
        log_debug("\tfree reverser board");
        free(reverser->board);
        reverser->board = NULL;
    }
    if (reverser->block != NULL) {
        log_debug("\tfree reverser block");
        free(reverser->block);
        reverser->block = NULL;
    }
    free(reverser);
}

void free_crossing(void *pointer) {
    t_config_crossing *crossing = (t_config_crossing *) pointer;
    if (crossing == NULL) {
        return;
    }
    if (crossing->id != NULL) {
        log_debug("free crossing: %s", crossing->id);
        free(crossing->id);
        crossing->id = NULL;
    }
    if (crossing->main_segment != NULL) {
        log_debug("\tfree crossing main segment");
        free(crossing->main_segment);
        crossing->main_segment = NULL;
    }
    free(crossing);
}

void free_signal_type(void *pointer) {
    t_config_signal_type *signal_type = (t_config_signal_type *) pointer;
    if (signal_type == NULL) {
        return;
    }
    if (signal_type->id != NULL) {
        log_debug("free signal type: %s", signal_type->id);
        free(signal_type->id);
        signal_type->id = NULL;
    }
    if (signal_type->initial != NULL) {
        log_debug("\tfree signal type initial");
        free(signal_type->initial);
        signal_type->initial = NULL;
    }
    if (signal_type->aspects != NULL) {
        log_debug("\tfree signal type aspects");
        for (int i = 0; i < signal_type->aspects->len; ++i) {
            free(g_array_index(signal_type->aspects, char *, i));
        }
        g_array_free(signal_type->aspects, true);
    }
    free(signal_type);
}

void free_composite_signal(void *pointer) {
    t_config_composite_signal *composite_signal = (t_config_composite_signal *) pointer;
    if (composite_signal == NULL) {
        return;
    }
    if (composite_signal->id != NULL) {
        log_debug("free composite signal: %s", composite_signal->id);
        free(composite_signal->id);
        composite_signal->id = NULL;
    }
    if (composite_signal->distant != NULL) {
        log_debug("\tfree composite signal distant");
        free(composite_signal->distant);
        composite_signal->distant = NULL;
    }
    if (composite_signal->entry != NULL) {
        log_debug("\tfree composite signal entry");
        free(composite_signal->entry);
        composite_signal->entry = NULL;
    }
    if (composite_signal->exit != NULL) {
        log_debug("\tfree composite signal exit");
        free(composite_signal->exit);
        composite_signal->exit = NULL;
    }
    if (composite_signal->block != NULL) {
        log_debug("\tfree composite signal block");
        free(composite_signal->block);
        composite_signal->block = NULL;
    }
    free(composite_signal);
}

void free_peripheral_type(void *pointer) {
    t_config_peripheral_type *peripheral_type = (t_config_peripheral_type *) pointer;
    if (peripheral_type == NULL) {
        return;
    }
    if (peripheral_type->id != NULL) {
        log_debug("free peripheral type: %s", peripheral_type->id);
        free(peripheral_type->id);
        peripheral_type->id = NULL;
    }
    if (peripheral_type->initial != NULL) {
        log_debug("\tfree peripheral type initial");
        free(peripheral_type->initial);
        peripheral_type->initial = NULL;
    }
    if (peripheral_type->aspects != NULL) {
        log_debug("\tfree peripheral type aspects");
        for (int i = 0; i < peripheral_type->aspects->len; ++i) {
            free(g_array_index(peripheral_type->aspects, char *, i));
        }
        g_array_free(peripheral_type->aspects, true);
    }
    free(peripheral_type);
}

void nullify_extras_config_tables(void) {
    tb_blocks = NULL;
    tb_reversers = NULL;
    tb_crossings = NULL;
    tb_signal_types = NULL;
    tb_composite_signals = NULL;
    tb_peripheral_types = NULL;
}

void extras_yaml_sequence_start(char *scalar) {
    log_debug("extras_yaml_sequence_start: %s", scalar);
    switch (extras_mapping) {
        case EXTRAS_ROOT:
            if (str_equal(scalar, "blocks") || str_equal(scalar, "platforms")) {
                extras_sequence = BLOCKS;
                if (tb_blocks == NULL)
                    tb_blocks = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_block);
                return;
            }

            if (str_equal(scalar, "reversers")) {
                extras_sequence = REVERSERS;
                tb_reversers = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_reverser);
                return;
            }

            if (str_equal(scalar, "crossings")) {
                extras_sequence = CROSSINGS;
                tb_crossings = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_crossing);
                return;
            }

            if (str_equal(scalar, "signaltypes")) {
                extras_sequence = SIGNAL_TYPES;
                tb_signal_types = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_signal_type);
                return;
            }

            if (str_equal(scalar, "compositions")) {
                extras_sequence = COMPOSITE_SIGNALS;
                tb_composite_signals = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_composite_signal);
                return;
            }
            if (str_equal(scalar, "peripheraltypes")) {
                extras_sequence = PERIPHERAL_TYPES;
                tb_peripheral_types = g_hash_table_new_full(g_str_hash, g_str_equal, free_extras_id_key, free_peripheral_type);
                return;
            }
            break;
        case BLOCK:
            if (str_equal(scalar, "main")) {
                extras_sequence = BLOCK_MAIN_SEGMENTS;
                cur_block->main_segments = g_array_sized_new(false, false, sizeof(char *), 2);
                return;
            }

            if (str_equal(scalar, "overlaps")) {
                extras_sequence = BLOCK_OVERLAPS;
                cur_block->overlaps = g_array_sized_new(false, false, sizeof(char *), 2);
                return;
            }

            if (str_equal(scalar, "signals")) {
                extras_sequence = BLOCK_SIGNALS;
                cur_block->signals = g_array_sized_new(false, false, sizeof(char *), 2);
                return;
            }

            if (str_equal(scalar, "trains")) {
                extras_sequence = BLOCK_TRAIN_TYPES;
                cur_block->train_types = g_array_sized_new(false, false, sizeof(char *), 8);
                return;
            }
            break;
        case SIGNAL_TYPE:
            if (str_equal(scalar, "initial")) {
                extras_sequence = SIGNAL_TYPE_INITIAL;
                return;
            }
            
			if (str_equal(scalar, "aspects")) {
                extras_sequence = SIGNAL_TYPE_ASPECTS;
                cur_signal_type->aspects = g_array_sized_new(false, false, sizeof(char *), 3);
                return;
            }
            break;
        case PERIPHERAL_TYPE:
			if (str_equal(scalar, "initial")) {
                extras_sequence = PERIPHERAL_TYPE_INITIAL;
                return;
            }
        
            if (str_equal(scalar, "aspects")) {
                extras_sequence = PERIPHERAL_TYPE_ASPECTS;
                cur_peripheral_type->aspects = g_array_sized_new(false, false, sizeof(char *), 3);
                return;
            }
            break;
        default:
            break;
    }
}

void extras_yaml_sequence_end(char *scalar) {
    log_debug("extras_yaml_sequence_end: %s", scalar);
    // decrease sequence level
    switch (extras_sequence) {
        case BLOCKS:
        case REVERSERS:
        case CROSSINGS:
        case SIGNAL_TYPES:
        case COMPOSITE_SIGNALS:
        case PERIPHERAL_TYPES:
            extras_sequence = EXTRAS_SEQ_NONE;
            break;
        case BLOCK_MAIN_SEGMENTS:
        case BLOCK_OVERLAPS:
        case BLOCK_SIGNALS:
        case BLOCK_TRAIN_TYPES:
            extras_sequence = BLOCKS;
            break;
        case SIGNAL_TYPE_ASPECTS:
            extras_sequence = SIGNAL_TYPES;
            break;
        case PERIPHERAL_TYPE_ASPECTS:
            extras_sequence = PERIPHERAL_TYPES;
            break;
        default:
            break;
    }
}

void extras_yaml_mapping_start(char *scalar) {
    log_debug("extras_yaml_mapping_start: %s", scalar);
    switch (extras_sequence) {
        case BLOCKS:
            extras_mapping = BLOCK;
            cur_block = malloc(sizeof(t_config_block));
            cur_block->id = NULL;
            cur_block->train_types = NULL;
            cur_block->signals = NULL;
            cur_block->main_segments = NULL;
            cur_block->overlaps = NULL;
            cur_block->is_reversed = false;
            cur_block->direction = NULL;
            break;
        case REVERSERS:
            extras_mapping = REVERSER;
            cur_reverser = malloc(sizeof(t_config_reverser));
            cur_reverser->id = NULL;
            cur_reverser->board = NULL;
            cur_reverser->block = NULL;
            break;
        case CROSSINGS:
            extras_mapping = CROSSING;
            cur_crossing = malloc(sizeof(t_config_crossing));
            cur_crossing->id = NULL;
            cur_crossing->main_segment = NULL;
            break;
        case SIGNAL_TYPES:
            extras_mapping = SIGNAL_TYPE;
            cur_signal_type = malloc(sizeof(t_config_signal_type));
            cur_signal_type->id = NULL;
            cur_signal_type->initial = NULL;
            cur_signal_type->aspects = NULL;
            break;
        case COMPOSITE_SIGNALS:
            extras_mapping = COMPOSITE_SIGNAL;
            cur_composite_signal = malloc(sizeof(t_config_composite_signal));
            cur_composite_signal->id = NULL;
            cur_composite_signal->entry = NULL;
            cur_composite_signal->exit = NULL;
            cur_composite_signal->block = NULL;
            cur_composite_signal->distant = NULL;
            break;
        case PERIPHERAL_TYPES:
            extras_mapping = PERIPHERAL_TYPE;
            cur_peripheral_type = malloc(sizeof(t_config_peripheral_type));
            cur_peripheral_type->id = NULL;
            cur_peripheral_type->initial = NULL;
            cur_peripheral_type->aspects = NULL;
            break;
        default:
            break;
    }
}

void extras_yaml_mapping_end(char *scalar) {
    log_debug("extras_yaml_mapping_end: %s", scalar);

    // insert mapping to hash table
    switch (extras_mapping) {
        case BLOCK:
            log_debug("extras_yaml_mapping_end: insert block: %s", cur_block->id);
            g_hash_table_insert(tb_blocks, strdup(cur_block->id), cur_block);
            break;
        case REVERSER:
            log_debug("extras_yaml_mapping_end: insert reverser: %s", cur_reverser->id);
            g_hash_table_insert(tb_reversers, strdup(cur_reverser->id), cur_reverser);
            break;
        case CROSSING:
            log_debug("extras_yaml_mapping_end: insert crossing: %s", cur_crossing->id);
            g_hash_table_insert(tb_crossings, strdup(cur_crossing->id), cur_crossing);
            break;
        case SIGNAL_TYPE:
            log_debug("extras_yaml_mapping_end: insert signal type: %s", cur_signal_type->id);
            g_hash_table_insert(tb_signal_types, strdup(cur_signal_type->id), cur_signal_type);
            break;
        case COMPOSITE_SIGNAL:
            log_debug("extras_yaml_mapping_end: insert composite signal: %s", cur_composite_signal->id);
            g_hash_table_insert(tb_composite_signals, strdup(cur_composite_signal->id), cur_composite_signal);
            break;
        case PERIPHERAL_TYPE:
            log_debug("extras_yaml_mapping_end: insert peripheral type: %s", cur_peripheral_type->id);
            g_hash_table_insert(tb_peripheral_types, strdup(cur_peripheral_type->id), cur_peripheral_type);
            break;
        default:
            break;
    }

    // decrease mapping level
    switch (extras_mapping) {
        case BLOCK:
        case REVERSER:
        case CROSSING:
        case SIGNAL_TYPE:
        case COMPOSITE_SIGNAL:
        case PERIPHERAL_TYPE:
            extras_mapping = EXTRAS_ROOT;
            break;
        default:
            break;
    }
}

void extras_yaml_scalar(char *last_scalar, char *cur_scalar) {
    if (extras_sequence == BLOCK_MAIN_SEGMENTS) {
        g_array_append_val(cur_block->main_segments, cur_scalar);
        return;
    }
    
    if (extras_sequence == BLOCK_OVERLAPS) {
        g_array_append_val(cur_block->overlaps, cur_scalar);
        return;
    }

    if (extras_sequence == BLOCK_SIGNALS) {
        g_array_append_val(cur_block->signals, cur_scalar);
        return;
    }

    if (extras_sequence == BLOCK_TRAIN_TYPES) {
        g_array_append_val(cur_block->train_types, cur_scalar);
        return;
    }

    if (extras_sequence == SIGNAL_TYPE_ASPECTS) {
        log_debug("insert aspect to signal type: %s, %s", cur_signal_type->id, cur_scalar);
        g_array_append_val(cur_signal_type->aspects, cur_scalar);
        return;
    }

    if (extras_sequence == PERIPHERAL_TYPE_ASPECTS) {
        log_debug("insert aspect to peripheral type: %s, %s", cur_peripheral_type->id, cur_scalar);
        g_array_append_val(cur_peripheral_type->aspects, cur_scalar);
        return;
    }

    switch (extras_mapping) {
        case BLOCK:
            if (str_equal(last_scalar, "id")) {
                cur_block->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "length")) {
                cur_block->length = strtof(cur_scalar, NULL);
                break;
            }

            if (str_equal(last_scalar, "limit")) {
                cur_block->limit_speed = strtof(cur_scalar, NULL);
                break;
            }
            
            if (str_equal(last_scalar, "is_reversed")) {
                cur_block->is_reversed = strcmp(cur_scalar, "true") == 0;
                break;
            }

            if (str_equal(last_scalar, "direction")) {
                cur_block->direction = cur_scalar;
                return;
            }
            break;
        case REVERSER:
            if (str_equal(last_scalar, "id")) {
                cur_reverser->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "board")) {
                cur_reverser->board = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "block")) {
                cur_reverser->block = cur_scalar;
                return;
            }
            break;
        case CROSSING:
            if (str_equal(last_scalar, "id")) {
                cur_crossing->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "segment")) {
                cur_crossing->main_segment = cur_scalar;
                return;
            }
            break;
        case SIGNAL_TYPE:
            if (str_equal(last_scalar, "id")) {
                cur_signal_type->id = cur_scalar;
                return;
            }
            
			if (str_equal(last_scalar, "initial")) {
                cur_signal_type->initial = cur_scalar;
                return;
            }
            break;
        case COMPOSITE_SIGNAL:
            if (str_equal(last_scalar, "id")) {
                cur_composite_signal->id = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "entry")) {
                cur_composite_signal->entry = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "exit")) {
                cur_composite_signal->exit = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "block")) {
                cur_composite_signal->block = cur_scalar;
                return;
            }

            if (str_equal(last_scalar, "distant")) {
                cur_composite_signal->distant = cur_scalar;
                return;
            }
            break;
        case PERIPHERAL_TYPE:
            if (str_equal(last_scalar, "id")) {
                cur_peripheral_type->id = cur_scalar;
                return;
            }

			if (str_equal(last_scalar, "initial")) {
                cur_peripheral_type->initial = cur_scalar;
                return;
            }
            break;
        default:
            break;
    }
    free(cur_scalar);
    cur_scalar = NULL;
}

void parse_extras_yaml(yaml_parser_t *parser, t_config_data *data) {
	extras_mapping = EXTRAS_ROOT;
    extras_sequence = EXTRAS_SEQ_NONE;
    
    parse_yaml_content(parser, extras_yaml_sequence_start, extras_yaml_sequence_end, extras_yaml_mapping_start, extras_yaml_mapping_end, extras_yaml_scalar);
    data->table_blocks = tb_blocks;
    data->table_reversers = tb_reversers;
    data->table_crossings = tb_crossings;
    data->table_signal_types = tb_signal_types;
    data->table_composite_signals = tb_composite_signals;
    data->table_peripheral_types = tb_peripheral_types;
}
