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

#ifndef SWTBAHN_CLI_AUTH_H
#define SWTBAHN_CLI_AUTH_H


#include <onion/types.h>

typedef struct {
    char* role;
    int current_platform;
    onion_handler_handler handler;
} auth_data;

/**
 * Wraps an onion handler with authentication and authorization using the HTTP Basic scheme.
 *
 * First authenticates the user against the user database, then checks if the user holds the appropriate role.
 * If current_platform is 1, the role has to be present for the currently active platform. Otherwise, the user has to
 * have the role generally (for platform '*').
 *
 * @param role the role to require the user to have.
 * @param current_platform requires the role to be present for the current platform.
 * @param handler the handler to wrap.
 *
 * @return the wrapped handler.
 *
 * @see handler_auth(void*, onion_request*, onion_response*)
 */
onion_handler* auth_require_role(char* role, int current_platform, onion_handler_handler handler);

/**
 * Handles authentication of requests.
 *
 * @param data authentication data.
 * @param req
 * @param res
 * @return
 *
 * @see auth_data
 */
onion_connection_status handler_auth(void* data, onion_request* req, onion_response* res);

/**
 * Handler returning information about the currently authenticated user.
 *
 * @param _
 * @param req
 * @param res
 * @return
 */
onion_connection_status handler_userinfo(void* _, onion_request* req, onion_response* res);

int authenticate(char* username, char* password);

void request_authentication(onion_response * res, char* message);

#endif //SWTBAHN_CLI_AUTH_H
