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

#ifndef CONFIG_DATA_INTERN_H
#define CONFIG_DATA_INTERN_H

#include <glib.h>
#include <stdbool.h>

typedef struct {
    GHashTable *table_segments;
    GHashTable *table_signals;
    GHashTable *table_points;
    GHashTable *table_peripherals;
    GHashTable *table_trains;
    GHashTable *table_blocks; // store: blocks and platforms
    GHashTable *table_reversers;
    GHashTable *table_crossings;
    GHashTable *table_signal_types;
    GHashTable *table_composite_signals;
    GHashTable *table_peripheral_types;
} t_config_data;

typedef struct {
    char *id;
    float length;
} t_config_segment;

typedef struct {
    char *id;
    char *board;
    char *block;
} t_config_reverser;

typedef struct {
    char *id;
    char *initial;
    GArray *aspects;
    char *type;
} t_config_signal;

typedef struct {
    char *id;
    char *initial;
    char *segment;
    char *normal_aspect;
    char *reverse_aspect;
} t_config_point;

typedef struct {
    char *id;
    char *initial;
    GArray *aspects;
    char *type;
} t_config_peripheral;

typedef struct {
    char *id;
    float length;
    float weight;
    char *type;
    GArray *peripherals;
    GArray *calibration;
} t_config_train;

typedef struct {
    char *id;
    float length;
    float limit_speed;
    GArray *train_types;
    GArray *signals;
    GArray *main_segments;
    GArray *overlaps;
    bool is_reversed;
    char *direction;
} t_config_block;

typedef struct {
    char *id;
    char *main_segment;
} t_config_crossing;

typedef struct {
    char *id;
    char *initial;
    GArray *aspects;
} t_config_signal_type;

typedef struct {
    char *id;
    char *distant;
    char *entry;
    char *exit;
    char *block;
} t_config_composite_signal;

typedef struct {
    char *id;
    char *initial;
    GArray *aspects;
} t_config_peripheral_type;


#endif //CONFIG_DATA_INTERN_H
