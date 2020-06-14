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

#include "handler_users.h"
#include <onion/types.h>
#include <onion/response.h>
#include <onion/request.h>
#include "auth_db.h"
#include "util_handler.h"

onion_connection_status handler_user_get_all(void *_, onion_request *req, onion_response *res) {
    int users_len;
    db_user** users = db_get_all_users(&users_len);
    if (users == NULL) {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "error: failed to load users from database");
        return OCS_PROCESSED;
    }

    onion_response_set_code(res, 200);
    onion_response_set_header(res, "content-type", "application/json");

    onion_response_printf(res, "[");

    for (int i = 0; i < users_len; ++i) {
        if (i != 0) {
            onion_response_printf(res, ",");
        }
        onion_response_printf(res, "{");
        db_user* user = users[i];

        onion_response_printf(res, "\"username\":\"%s\",", user->username);
        if (user->mail == NULL) {
            onion_response_printf(res, "\"mail\":null,");
        } else {
            onion_response_printf(res, "\"mail\":\"%s\",", user->mail);
        }
        onion_response_printf(res, "\"active\":%s,", user->active ? "true" : "false");
        onion_response_printf(res, "\"roles\":[");

        for (int j = 0; j < user->numRoles; ++j) {
            if (j != 0) {
                onion_response_printf(res, ",");
            }
            onion_response_printf(res, "\"%s\"", user->roles[j]);
        }

        onion_response_printf(res, "]}");
    }

    onion_response_printf(res, "]");

    db_free_all_users(users, users_len);
    return OCS_PROCESSED;
}

onion_connection_status handler_user_activate(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");

    if (db_user_activate(user, 1)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to update user.");
    }
    return OCS_PROCESSED;
}

onion_connection_status handler_user_deactivate(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");

    if (db_user_activate(user, 0)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to update user.");
    }
    return OCS_PROCESSED;
}

onion_connection_status handler_user_add_role(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");
    char* role = onion_request_get_post(req, "role");

    if (db_user_has_role(user, role)) {
        onion_response_set_code(res, 400);
        onion_response_printf(res, "User already has this role.");
        return OCS_PROCESSED;
    }

    if (db_user_add_role(user, role)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to update user.");
    }
    return OCS_PROCESSED;
}

onion_connection_status handler_user_remove_role(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");
    char* role = onion_request_get_post(req, "role");

    if (!db_user_has_role(user, role)) {
        onion_response_set_code(res, 404);
        onion_response_printf(res, "User does not have that role.");
        return OCS_PROCESSED;
    }

    if (db_user_remove_role(user, role)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to update user.");
    }
    return OCS_PROCESSED;
}

onion_connection_status handler_user_delete(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");

    if (db_user_remove(user)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to delete user.");
    }

    return OCS_PROCESSED;
}

onion_connection_status handler_user_register(void *_, onion_request *req, onion_response *res) {
    if (request_require_post(req, res)) {
        return OCS_PROCESSED;
    }

    char* user = onion_request_get_post(req, "user");
    char* mail = onion_request_get_post(req, "mail");
    char* password = onion_request_get_post(req, "password");

    if (db_user_exists(user)) {
        // TODO: this should actually be rate limited or something to defend against enumeration attacks
        onion_response_set_code(res, 400);
        onion_response_printf(res, "Username already exists.");
        return OCS_PROCESSED;
    }

    if (db_create_user(user, password, mail, 0)) {
        onion_response_set_code(res, 200);
    } else {
        onion_response_set_code(res, 500);
        onion_response_printf(res, "Failed to create user.");
    }
}
