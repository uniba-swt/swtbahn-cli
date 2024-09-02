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

#include "config_data_parser.h"
#include "parser_util.h"
#include "track_config_parser.h"
#include "train_config_parser.h"
#include "extras_config_parser.h"

const char TRACK_CONFIG_FILENAME[] = "bidib_track_config.yml";
const char TRAIN_CONFIG_FILENAME[] = "bidib_train_config.yml";
const char EXTRAS_CONFIG_FILENAME[] = "extras_config.yml";

bool parse_config(const char *config_dir, const char *filename, t_config_data *data, void (*parse_yaml)(yaml_parser_t *, t_config_data *)) {
    // init
    FILE *fh;
    yaml_parser_t parser;
    if (!init_config_parser(config_dir, filename, &fh, &parser)) {
        return false;
    }

    // parse
    parse_yaml(&parser, data);

    // destroy
    destroy_parser(fh, &parser);
    return true;
}

bool parse_config_data(const char *config_dir, t_config_data *config_data) {

    if (!parse_config(config_dir, TRACK_CONFIG_FILENAME, config_data, parse_track_yaml)) {
        syslog_server(LOG_ERR, "Failed to parse %s", TRACK_CONFIG_FILENAME);
        return false;
    }

    if (!parse_config(config_dir, TRAIN_CONFIG_FILENAME, config_data, parse_train_yaml)) {
        syslog_server(LOG_ERR, "Failed to parse %s", TRAIN_CONFIG_FILENAME);
        return false;
    }

    if (!parse_config(config_dir, EXTRAS_CONFIG_FILENAME, config_data, parse_extras_yaml)) {
        syslog_server(LOG_ERR, "Failed to parse %s", EXTRAS_CONFIG_FILENAME);
        return false;
    }

    return true;
}

void free_config_data(t_config_data config_data) {
    if (config_data.table_segments != NULL) {
        g_hash_table_destroy(config_data.table_segments);
        config_data.table_segments = NULL;
    }

    if (config_data.table_signals != NULL) {
        g_hash_table_destroy(config_data.table_signals);
        config_data.table_signals = NULL;
    }

    if (config_data.table_points != NULL) {
        g_hash_table_destroy(config_data.table_points);
        config_data.table_points = NULL;
    }
    
    if (config_data.table_peripherals != NULL) {
        g_hash_table_destroy(config_data.table_peripherals);
        config_data.table_peripherals = NULL;
    }

    nullify_track_config_tables();
    
    if (config_data.table_trains != NULL) {
        g_hash_table_destroy(config_data.table_trains);
        config_data.table_trains = NULL;
    }
    
    nullify_train_config_table();

    if (config_data.table_blocks != NULL) {
        g_hash_table_destroy(config_data.table_blocks);
        config_data.table_blocks = NULL;
    }

    if (config_data.table_reversers != NULL) {
        g_hash_table_destroy(config_data.table_reversers);
        config_data.table_reversers = NULL;
    }

    if (config_data.table_crossings != NULL) {
        g_hash_table_destroy(config_data.table_crossings);
        config_data.table_crossings = NULL;
    }
    
    if (config_data.table_signal_types != NULL) {
        g_hash_table_destroy(config_data.table_signal_types);
        config_data.table_signal_types = NULL;
    }

    if (config_data.table_composite_signals != NULL) {
        g_hash_table_destroy(config_data.table_composite_signals);
        config_data.table_composite_signals = NULL;
    }
    
    if (config_data.table_peripheral_types != NULL) {
        g_hash_table_destroy(config_data.table_peripheral_types);
        config_data.table_peripheral_types = NULL;
    }
    
    nullify_extras_config_tables();
    
    syslog_server(LOG_NOTICE, "Config data freed");
}
