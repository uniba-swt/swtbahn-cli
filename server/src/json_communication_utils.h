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

#ifndef JSON_COMMUNICATION_UTILS_H
#define JSON_COMMUNICATION_UTILS_H

#include <glib.h>
#include <stdbool.h>
#include <onion/types.h>

//########################## COMMON UTIL ##########################

/**
 * @brief Build a json object (in form of a string) containing
 * the fields "success" with a boolean value and "message" with a string
 * value.
 * @attention the returned GString * has to be freed/managed by the caller.
 * 
 * @param success value of the boolean "success" field in the json object.
 * @param message value of the string "message" field in the json object. 
 * If NULL is passed, the field will be empty.
 * @return GString* json object string with success and message field.
 */
GString *get_common_feedback_json(bool success, const char *message);

/**
 * @brief Build a json object (in form of a string) containing
 * the field "err_message" with a string value, value is content of err_message parameter.
 * @attention the returned GString * has to be freed/managed by the caller.
 * 
 * @param err_message the error message value for the "err_message" field in the json object.
 * If NULL is passed, the field will be empty.
 * @return GString* json object string with the err_message field.
 */
GString *get_common_error_message_json(const char *err_message);

/**
 * @brief Constructs the common-feedback json and sends it via the response res.
 * 
 * @param res the response over which to send. Shall not be NULL, otherwise no sending takes place.
 * @param success intended value of the boolean "success" field in the json to be sent
 * @param message intended value of the string "message" field in the json to be sent.
 * If message is NULL, the message field will be empty.
 * @return true if parameters were valid
 * @return false if parameters were invalid or an internal error occurred (nothing will be sent)
 */
bool send_common_feedback(onion_response *res, bool success, const char* message);

/**
 * @brief Constructs the common error message json and sends it via the response res.
 * 
 * @param res the response over which to send. Shall not be NULL, otherwise no sending takes place.
 * @param err_message intended value of the string "err_message" field in the json to be sent.
 * If err_message is NULL, the err_message field will be empty.
 * @return true if parameters were valid
 * @return false if parameters were invalid or an internal error occurred (nothing will be sent)
 */
bool send_common_error_message(onion_response *res, const char* err_message);

/**
 * @brief Sends the content of gstr via the response res. Then free's the GString,
 * i.e., this is a transfer of ownership.
 * 
 * @param res the response over which to send. Shall not be NULL.
 * @param gstr the gstring whose contents to send. Shall not be NULL. 
 * Will be free'd if it is not null, regardless of whether true or false is returned.
 * @return true if parameters were valid and sending succeeded,
 * @return false otherwise.
 */
bool send_some_gstring(onion_response *res, GString *gstr);

#endif // JSON_COMMUNICATION_UTILS_H