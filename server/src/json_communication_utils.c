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

#include "json_communication_utils.h"
#include "json_response_builder.h"
#include <onion/response.h>

GString *get_common_feedback_json(bool success, const char* message) {
	const gsize add_len = message != NULL ? MAX(256, strlen(message)) : 0;
	GString *g_feedback = g_string_sized_new(32 + add_len);
	g_string_assign(g_feedback, "");
	append_start_of_obj(g_feedback, false);
	append_field_bool_value(g_feedback, "success", success, true);
	append_field_str_value(g_feedback, "message", message != NULL ? message : "", false);
	append_end_of_obj(g_feedback, false);
	return g_feedback;
}

GString *get_common_error_message_json(const char *err_message) {
	const gsize add_len = err_message != NULL ? MAX(256, strlen(err_message)) : 0;
	GString *g_err_msg = g_string_sized_new(24 + add_len);
	g_string_assign(g_err_msg, "");
	append_start_of_obj(g_err_msg, false);
	append_field_str_value(g_err_msg, "err_message", err_message != NULL ? err_message : "", false);
	append_end_of_obj(g_err_msg, false);
	return g_err_msg;
}

bool send_common_feedback(onion_response *res, bool success, const char* message) {
	if (res == NULL) {
		return false;
	}
	GString *ret_str = get_common_feedback_json(success, message);
	if (ret_str == NULL) {
		return false;
	}
	bool ret = onion_response_printf(res, "%s", ret_str->str) >= 0;
	g_string_free(ret_str, true);
	return ret;
}

bool send_common_error_message(onion_response *res, const char* err_message) {
	if (res == NULL) {
		return false;
	}
	GString *ret_str = get_common_error_message_json(err_message);
	if (ret_str == NULL) {
		return false;
	}
	bool ret = onion_response_printf(res, "%s", ret_str->str) >= 0;
	g_string_free(ret_str, true);
	return ret;
}

bool send_some_gstring(onion_response *res, GString *gstr) {
	if (res == NULL) {
		if (gstr != NULL) {
			g_string_free(gstr, true);
		}
		return false;
	} else if (gstr == NULL) {
		return false;
	}
	bool ret = onion_response_printf(res, "%s", gstr->str) >= 0;
	g_string_free(gstr, true);
	gstr = NULL;
	return ret;
}
