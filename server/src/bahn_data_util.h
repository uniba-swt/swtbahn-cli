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

#ifndef BAHN_DATA_UTIL_H
#define BAHN_DATA_UTIL_H

#include <stdbool.h>

/**
 * @brief Initialize the interlocking table and load/parse config files from the config-directory.
 * 
 * @param config_dir directory containing the config files
 * @return true if initialization succeeded, otherwise false
 */
bool bahn_data_util_initialise_config(const char *config_dir);

/**
 * @brief Free all loaded config data (incl. interlocking table)
 * 
 */
void bahn_data_util_free_config();

/**
 * @brief Returns true if str1 is (lexic.) equal to str2.
 * Returns false if either parameter is NULL.
 * 
 */
bool string_equals(const char *str1, const char *str2);

/**
 * @brief Initialize the cached-track-state, has to be called before one or more calls to 
 * `track_state_get_value`.
 * 
 */
void bahn_data_util_init_cached_track_state();

/**
 * @brief Frees the cached-track-state, has to be called after one or more calls to 
 * `track_state_get_value` (invalidates/frees any strings returned by track_state_get_value).
 * 
 */
void bahn_data_util_free_cached_track_state();

/**
 * @brief Get the route ids of routes that start at a specific source signal and end at a specific
 * destination signal. Resulting list/array of route ids is written to out-param `route_ids`.
 * Caller is responsible to ensure `route_ids` is big enough to hold all ids.
 * 
 * @param src_signal_id source signal
 * @param dst_signal_id destination signal
 * @param route_ids out-parameter, ids of routes...
 * @return int amount of routes found and written to route_ids.
 */
int interlocking_table_get_routes(const char *src_signal_id, const char *dst_signal_id, char *route_ids[]);

char *config_get_scalar_string_value(const char *type, const char *id, const char *prop_name);

int config_get_scalar_int_value(const char *type, const char *id, const char *prop_name);

float config_get_scalar_float_value(const char *type, const char *id, const char *prop_name);

bool config_get_scalar_bool_value(const char *type, const char *id, const char *prop_name);

int config_get_array_string_value(const char *type, const char *id, const char *prop_name, char *data[]);

int config_get_array_int_value(const char *type, const char *id, const char *prop_name, int data[]);

int config_get_array_float_value(const char *type, const char *id, const char *prop_name, float data[]);

int config_get_array_bool_value(const char *type, const char *id, const char *prop_name, bool data[]);

bool config_set_scalar_string_value(const char *type, const char *id, const char *prop_name, char *value);

char *track_state_get_value(const char *id);

bool track_state_set_value(const char *id, const char *value);

bool is_segment_occupied(const char *id);

bool is_type_segment(const char *id);

bool is_type_signal(const char *id);

bool is_type_point(const char *id);

int train_state_get_speed(const char *train_id);

bool train_state_set_speed(const char *train_id, int speed);

bool train_known(const char *train_id);

char *config_get_point_position(const char *route_id, const char *point_id);

char *config_get_block_id_of_segment(const char *seg_id);

char *config_get_module_name();

void log_bool(bool value);

void log_int(int value);

void log_float(float value);

void log_string(const char *value);

#endif  // BAHN_DATA_UTIL_H