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
#include <stdio.h>
#include <stdarg.h>
#include "parser_util.h"
#include "../server.h"

void log_debug(const char *format, ...) {
    char string[1024];
    va_list arg;
    va_start(arg, format);
    vsnprintf(string, 1024, format, arg);

    syslog(LOG_DEBUG, "server: %s", string);
}

bool init_config_parser(const char *config_dir, const char *table_file, FILE **fh, yaml_parser_t *parser) {
    // generate full path
    size_t len = strlen(config_dir) + strlen(table_file) + 1;
    char full_path[len];
    snprintf(full_path, len, "%s%s", config_dir, table_file);

    // init file
    *fh = fopen(full_path, "r");

    if (*fh == NULL) {
        syslog_server(LOG_ERR, "YAML parser: Failed to open %s", table_file);
        return false;
    }

    if (!yaml_parser_initialize(parser)) {
        fclose(*fh);
        syslog_server(LOG_ERR, "YAML parser: Failed to initialise parser");
        return false;
    }

    yaml_parser_set_input_file(parser, *fh);
    return true;
}

void destroy_parser(FILE *fh, yaml_parser_t *parser) {
    yaml_parser_delete(parser);
    fclose(fh);
}

void parse_yaml_content(yaml_parser_t *parser,
                        void(*sequence_start_handler)(char*),
                        void(*sequence_end_handler)(char*),
                        void(*mapping_start_handler)(char*),
                        void(*mapping_end_handler)(char*),
                        void(*scalar_handler)(char*, char*)) {
    yaml_event_t  event;
    bool error = false;
    char *cur_scalar = NULL;
    char *last_scalar = NULL;

    do {
        if (!yaml_parser_parse(parser, &event)) {
            syslog_server(LOG_ERR, "Error parsing: %d\n", (*parser).error);
            break;
        }

        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        switch(event.type) {
            case YAML_NO_EVENT:
                error = true;
                break;

                /* Stream start/end */
            case YAML_STREAM_START_EVENT:
                break;
            case YAML_STREAM_END_EVENT:
                break;

                /* Block delimeters */
            case YAML_DOCUMENT_START_EVENT:
                break;
            case YAML_DOCUMENT_END_EVENT:
                break;

                /* Array */
            case YAML_SEQUENCE_START_EVENT:
                sequence_start_handler(last_scalar);
                break;
            case YAML_SEQUENCE_END_EVENT:
                sequence_end_handler(last_scalar);
                break;

                /* Object */
            case YAML_MAPPING_START_EVENT:
                mapping_start_handler(last_scalar);
                break;
            case YAML_MAPPING_END_EVENT:
                mapping_end_handler(last_scalar);
                break;

                /* Data */
            case YAML_ALIAS_EVENT:
                error = true;
                break;
            case YAML_SCALAR_EVENT:
                cur_scalar = strdup((char*)event.data.scalar.value);
                if (last_scalar == NULL) {
                    break;
                }
                scalar_handler(last_scalar, strdup(cur_scalar));

                break;
            default:
                error = true;
                break;
        }
        yaml_event_delete(&event);
        ///TODO: FIXME
        if (last_scalar != NULL) {
            free(last_scalar);
            last_scalar = NULL;
        }
        last_scalar = cur_scalar;
    } while(!error);
}

bool str_equal(const char *str1, const char *str2) {
    return strcmp(str1, str2) == 0;
}

float parse_float(const char *str) {
    return strtof(str, NULL);
}
