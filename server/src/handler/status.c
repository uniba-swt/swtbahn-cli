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
 * - Florian Beetz <https://github.com/florian-beetz>
 *
 */

#include <onion/response.h>
#include "status.h"
#include "admin.h"
#include "dirent.h"
#include "../server.h"

onion_connection_status handler_platform_get(void *_, onion_request *req, onion_response *res) {
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(config_directory)) == NULL) {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "failed to read config directory.");
        return OCS_PROCESSED;
    }

    onion_response_set_code(res, 200);
    onion_response_set_header(res, "content-type", "application/json");
    onion_response_printf(res, "[");

    int first = 1;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_DIR || strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        if (!first) {
            onion_response_printf(res, ",\"%s\"", ent->d_name);
        } else {
            onion_response_printf(res, "\"%s\"", ent->d_name);
            first = 0;
        }
    }
    closedir(dir);

    onion_response_printf(res, "]");

    return OCS_PROCESSED;
}

onion_connection_status handler_platform_current(void *_, onion_request *req, onion_response *res) {
    if (platform_loaded != NULL) {
        onion_response_set_code(res, 200);
        onion_response_printf(res, "%s", platform_loaded);
    } else {
        onion_response_set_code(res, 404);
    }
    return OCS_PROCESSED;
}

