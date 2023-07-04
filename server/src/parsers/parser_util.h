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

#ifndef PARSER_UTIL_H
#define PARSER_UTIL_H

#include <syslog.h>
#include <stdbool.h>
#include <yaml.h>
#include <stdio.h>
#include "../server.h"

void log_debug(const char *format, ...);

bool init_config_parser(const char *config_dir, const char *table_file, FILE **fh, yaml_parser_t *parser);

void destroy_parser(FILE *fh, yaml_parser_t *parser);

void parse_yaml_content(yaml_parser_t *parser,
                        void(*sequence_start_handler)(char*),
                        void(*sequence_end_handler)(char*),
                        void(*mapping_start_handler)(char*),
                        void(*mapping_end_handler)(char*),
                        void(*scalar_handler)(char*, char*));

bool str_equal(const char *str1, const char *str2);

float parse_float(const char *str);

#endif  // PARSER_UTIL_H