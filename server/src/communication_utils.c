/*
 *
 * Copyright (C) 2024 University of Bamberg, Software Technologies Research Group
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
 * - Bernhard Luedtke
 *
 */

#include "communication_utils.h"
#include "json_response_builder.h"

#include "server.h" // for logging

#include <onion/response.h>

/*
GString *get_common_feedback_json(const char* message) {
	const gsize add_len = message != NULL ? MAX(256, strlen(message)) : 0;
	GString *g_feedback = g_string_sized_new(32 + add_len);
	g_string_assign(g_feedback, "");
	append_start_of_obj(g_feedback, false);
	append_field_str_value(g_feedback, "msg", message != NULL ? message : "", false);
	append_end_of_obj(g_feedback, false);
	return g_feedback;
}*/

bool send_common_feedback(onion_response *res, int status_code, const char* message) {
	if (res == NULL) {
		return false;
	}
	onion_response_set_code(res, status_code);
	if (message != NULL && strlen(message) > 0) {
		return onion_response_printf(res, "{\n\"msg\":\"%s\"\n}", message) >= 0;
	} else {
		return true;
	}
}

bool send_some_gstring(onion_response *res, int status_code, GString *gstr) {
	if (res == NULL) {
		if (gstr != NULL) {
			g_string_free(gstr, true);
		}
		return false;
	}
	onion_response_set_code(res, status_code);
	if (gstr == NULL) {
		return true;
	}
	bool ret = onion_response_printf(res, "%s", gstr->str) >= 0;
	g_string_free(gstr, true);
	gstr = NULL;
	return ret;
}

bool send_some_cstring(onion_response *res, int status_code, const char *cstr) {
	if (res == NULL) {
		return false;
	}
	onion_response_set_code(res, status_code);
	if (cstr == NULL) {
		return true;
	} else {
		return onion_response_printf(res, "%s", cstr) >= 0;
	}
}

onion_connection_status 
handle_req_run_or_method_fail(onion_response *res, bool is_running, const char *caller_logname) {
	if (is_running) {
		syslog_server(LOG_WARNING, "Request: %s - wrong request type", caller_logname);
		onion_response_set_code(res, HTTP_METHOD_NOT_ALLOWED);
		return OCS_NOT_IMPLEMENTED;
	} else {
		syslog_server(LOG_ERR, "Request: %s - system not running", caller_logname);
		onion_response_set_code(res, HTTP_SERVICE_UNAVAILABLE);
		return OCS_PROCESSED;
	}
}
