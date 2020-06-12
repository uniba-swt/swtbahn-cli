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
    onion_handler_handler handler;
} auth_data;

onion_handler* auth_require_role(char* role, onion_handler_handler handler);

onion_connection_status handler_auth(void* data, onion_request* req, onion_response* res);

onion_connection_status handler_userinfo(void* _, onion_request* req, onion_response* res);

int authenticate(char* username, char* password);

int user_has_role(char* username, char* role);

void request_authentication(onion_response * res, char* message);

#endif //SWTBAHN_CLI_AUTH_H
