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

#include "auth.h"
#include <stdlib.h>
#include <onion/handler.h>
#include <string.h>
#include <onion/response.h>
#include <onion/codecs.h>
#include "auth_db.h"

onion_handler *auth_require_role(char *role, onion_handler_handler handler) {
    auth_data* data = malloc(sizeof(auth_data));
    data->role = role;
    data->handler = handler;

    return onion_handler_new(&handler_auth, data, 0); // TODO: how to implement a thread-safe priv_data_free?
}

onion_connection_status handler_auth(void* data, onion_request* req, onion_response* res) {
    auth_data* authData = data;

    const char* authorization = onion_request_get_header(req, "Authorization");
    if (!authorization || strncmp(authorization, "Basic", 5) != 0) {
        request_authentication(res, "HTTP Basic Authentication required");
        return OCS_PROCESSED;
    }

    char* auth = onion_base64_decode(&authorization[6], NULL);

    // split username and password
    char* username = auth;
    char* password = NULL;
    int i = 0;
    while (auth[i] != '\0' && auth[i] != ':') {
        i++;
    }
    if (auth[i] == ':') {
        auth[i] = '\0'; // terminate username
        password = &auth[i+1];
    }

    // check valid credentials
    int authenticated = authenticate(username, password);
    if (!authenticated) {
        request_authentication(res, "Invalid credentials");
        return OCS_PROCESSED;
    }

    // check required role
    int authorized = user_has_role(username, authData->role);
    if (!authorized) {
        onion_response_set_code(res, 403);
        onion_response_printf(res, "Missing required role to perform action.");
        return OCS_PROCESSED;
    }

    return authData->handler(NULL, req, res);
}

onion_connection_status handler_userinfo(void *_, onion_request *req, onion_response *res) {
    const char* authorization = onion_request_get_header(req, "Authorization");
    if (!authorization || strncmp(authorization, "Basic", 5) != 0) {
        onion_response_set_code(res, 200);
        onion_response_set_header(res, "content-type", "application/json");
        onion_response_printf(res, "{\"loggedIn\": false, \"username\": null}");
        return OCS_PROCESSED;
    }

    char* auth = onion_base64_decode(&authorization[6], NULL);

    // split username and password
    char* username = auth;
    char* password = NULL;
    int i = 0;
    while (auth[i] != '\0' && auth[i] != ':') {
        i++;
    }
    if (auth[i] == ':') {
        auth[i] = '\0'; // terminate username
        password = &auth[i+1];
    }

    // check valid credentials
    int authenticated = authenticate(username, password);
    if (!authenticated) {
        onion_response_set_code(res, 200);
        onion_response_set_header(res, "content-type", "application/json");
        onion_response_printf(res, "{\"loggedIn\": false, \"username\": null}");
        return OCS_PROCESSED;
    }

    onion_response_set_code(res, 200);
    onion_response_set_header(res, "content-type", "application/json");
    onion_response_printf(res, "{\"loggedIn\": true, \"username\": \"%s\"}", username);
    return OCS_PROCESSED;
}

int authenticate(char *username, char *password) {
    return db_check_login(username, password);
}

int user_has_role(char *username, char *role) {
    return db_user_has_role(username, role);
}

void request_authentication(onion_response *res, char *message) {
    onion_response_set_code(res, 401);
    // since login is handled by the JS client, don't actually request the browser to authenticate the user
    //onion_response_set_header(res, "WWW-Authenticate", "Basic realm=\"SWTBahn\"");
    onion_response_printf(res, "%s", message);
}
