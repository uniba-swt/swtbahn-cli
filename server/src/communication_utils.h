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

#ifndef JSON_COMMUNICATION_UTILS_H
#define JSON_COMMUNICATION_UTILS_H

#include <glib.h>
#include <stdbool.h>
#include <onion/types.h>

// The onion library we use has no define for the 409 code. Add it here.
#define CUSTOM_HTTP_CODE_CONFLICT 409

/**
 * @brief Constructs the common-feedback json and sends it via the response res.
 * 
 * @param res the response over which to send. Shall not be NULL, otherwise no sending takes place.
 * @param status_code http status code to set for the response to be sent
 * @param message intended value of the string "msg" field in the json to be sent.
 * If message is NULL or empty, no response will be sent at all, just the code will be set.
 * @return true if parameters were valid
 * @return false if parameters were invalid or an internal error occurred (nothing will be sent)
 */
bool send_common_feedback(onion_response *res, int status_code, const char* message);

/**
 * @brief Constructs the common-feedback json with field prefilled for a missing parameter msg, 
 * and sends it via the response res. 
 * 
 * @param res the response over which to send. Shall not be NULL, otherwise no sending takes place.
 * @param status_code http status code to set for the response to be sent
 * @param param_name name of the missing parameter
 * @return true if parameters were valid
 * @return false if parameters were invalid or an internal error occurred (nothing will be sent)
 */
bool send_common_feedback_miss_param_helper(onion_response *res, int status_code, const char* param_name);

/**
 * @brief Checks if a parameter is missing by way of checking if param_value is NULL.
 * If it is NULL, sends common response with parameter missing message, and logs to syslog
 * 
 * @param res the response over which to send if the parameter is missing.
 * @param request_log_name the request calling this check, used for logging
 * @param param_name name of the parameter being checked
 * @param param_value value of the parameter
 * @return true if the parameter (value) is missing/NULL
 * @return false otherwise.
 */
bool handle_param_miss_check(onion_response *res, const char *request_log_name, 
                             const char *param_name, const char *param_value);

/**
 * @brief Sends the content of gstr via the response res. Then free's the GString,
 * i.e., this is a transfer of ownership.
 * 
 * @param res the response over which to send. Shall not be NULL.
 * @param status_code http status code to set for the response to be sent
 * @param gstr the gstring whose contents to send. If it is NULL, only the code will be set, nothing
 * will be sent. The gstr gstring will be free'd if it is not null, regardless of the return value.
 * @return true if parameters were valid and sending succeeded,
 * @return false otherwise.
 */
bool send_some_gstring_and_free(onion_response *res, int status_code, GString *gstr);

/**
 * @brief Constructs and sends a json with a single field, named by field_name, and its string value 
 * given by field_value. Sets passed status code before sending anything.
 * Note: if the field value is empty, nothing will be sent (only the status code will be set).
 * 
 * @param res the response over which to send. Shall not be NULL.
 * @param status_code http status code to set for the response to be sent
 * @param field_name the field identifier
 * @param field_value the field value
 * @return true if parameters were valid
 * @return false otherwise.
 */
bool send_single_str_field_feedback(onion_response *res, int status_code, const char* field_name, 
                                    const char* field_value);

// This does not free the passed string to be sent (in contrast to the gstring version).
/**
 * @brief Like send_some_gstring_and_free, but with a c-style 0-terminated string,
 * but does NOT free the string.
 * 
 * @param res the response over which to send. Shall not be NULL.
 * @param status_code http status code to set for the response to be sent
 * @param cstr the c-style 0-terminated string whose contents to send. 
 * If this is NULL, only the code will be set, nothing will be sent.
 * @return true 
 * @return false 
 */
bool send_some_cstring(onion_response *res, int status_code, const char *cstr);

/**
 * @brief Helper for logging to syslog and responding to request
 * that either the system is not running (but it has to be running for the request in question)
 * or that an incorrect HTTP method was used.
 * 
 * @param res the response over which to send. Shall not be NULL.
 * @param is_running whether the system is running or not. Pass `true` if this does not matter
 * for the request in question.
 * @param caller_logname name of the request endpoint to log
 * @return onion_connection_status OCS_NOT_IMPLEMENTED if wrong request type,
 * otherwise OCS_PROCESSED
 */
onion_connection_status handle_req_run_or_method_fail(onion_response *res, bool is_running, 
                                                      const char *caller_logname);

#endif // JSON_COMMUNICATION_UTILS_H