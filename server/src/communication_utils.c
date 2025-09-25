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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 *
 */

#include "communication_utils.h"

#include "server.h" // for logging

#include <onion/response.h>

bool send_common_feedback(onion_response *res, int status_code, const char* message) {
	return send_single_str_field_feedback(res, status_code, "msg", message);
}

bool send_common_feedback_miss_param_helper(onion_response *res, int status_code, const char* param_name) {
	if (res == NULL) {
		return false;
	}
	onion_response_set_code(res, status_code);
	if (param_name != NULL && strlen(param_name) > 0) {
		return onion_response_printf(res, "{\"msg\":\"missing parameter %s\"}", param_name) >= 0;
	} else {
		return true;
	}
}

/// NOTE: This function / helper might better fit into a general util class for handlers,
///       but we don't have that.
bool handle_param_miss_check(onion_response *res, const char *request_log_name, 
                             const char *param_name, const char *param_value) {
	if (param_value != NULL) {
		return false;
	}
	if (send_common_feedback_miss_param_helper(res, HTTP_BAD_REQUEST, param_name)) {
		syslog_server(LOG_ERR, "Request: %s - missing parameter %s", request_log_name, param_name);
	} else {
		syslog_server(LOG_ERR, 
		              "Request: %s - missing parameter %s - but sending msg to client failed", 
		              request_log_name, param_name);
	}
	return true;
}

bool send_some_gstring_and_free(onion_response *res, int status_code, GString *gstr) {
	if (res == NULL) {
		if (gstr != NULL) {
			g_string_free(gstr, true);
			gstr = NULL;
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

bool send_single_str_field_feedback(onion_response *res, int status_code, const char* field_name, 
                                    const char* field_value) {
	if (res == NULL) {
		return false;
	}
	onion_response_set_code(res, status_code);
	if (field_name != NULL && strlen(field_name) > 0 
		&& field_value != NULL && strlen(field_value) > 0) {
		return onion_response_printf(res, "{\"%s\":\"%s\"}", field_name, field_value) >= 0;
	} else {
		return true;
	}
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

onion_connection_status handle_req_run_or_method_fail(onion_response *res, bool is_running, 
                                                      const char *caller_logname) {
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
