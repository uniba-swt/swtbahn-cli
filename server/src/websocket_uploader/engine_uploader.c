/*
 *
 * Copyright (C) 2023 University of Bamberg, Software Technologies Research Group
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

#include "engine_uploader.h"
#include "../server.h"

#include <glib.h>
#include "mongoose.h"

typedef struct {
	bool started;
	bool finished;
	bool success;
	bool message_is_json_str;
	GString* file_path;
	GString* message;
} ws_verif_data;

///TODO: Protect against concur access with a mutex
char *verifier_url = NULL;

static const char cache_file_verifier_url[] = "verifier_url_cache.txt";

static const unsigned int websocket_single_poll_length_ms = 250;
static const unsigned int websocket_max_polls_before_start = 60;

static const char msg_type_field_key[] = "\"__MESSAGE_TYPE__\"";
static const char msg_type_start_sig[] = "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_START\"";
static const char msg_type_start_sctx_field_key[] = "\"sctx\"";
static const char msg_type_received_sig[] = "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RECEIVED\"";
static const char msg_type_result_sig[] = "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RESULT\"";
static const char msg_type_result_status_true_sig[] = "\"status\":true";
static const char msg_type_result_status_false_sig[] = "\"status\":false";

bool parse_model_into_verif_msg_str(GString *destination, const char *model_file_path);
void process_verif_server_reply(struct mg_ws_message *ws_msg, ws_verif_data* ws_data_ptr);
void send_verif_req_message(struct mg_connection *ws_connection, ws_verif_data* ws_data_ptr);
void websocket_verification_callback(struct mg_connection *ws_connection, int ev, void *ev_data, void *fn_data);


/**
 * @brief Creates a string that contains a message requesting a
 * verification server to verify the model found at model_file_path.
 * 
 * @param destination The GString that the message will be appended to.
 * @param model_file_path The path to the model to request verification for.
 * @return true if loading and parsing the model and building the message string succeeded.
 * @return false otherwise.
 */
bool parse_model_into_verif_msg_str(GString *destination, const char *model_file_path) {
	if (destination == NULL || model_file_path == NULL) {
		return false;
	}
	bool parse_success = false;
	
	FILE *fp = fopen(model_file_path, "r");
	if (fp) {
		// Adapted from https://stackoverflow.com/a/174743
		// Read entire file into buffer
		char *buffer = NULL;
		size_t length = 0;
		ssize_t bytes_read_count = getdelim(&buffer, &length, '\0', fp);
		if (bytes_read_count != -1 && buffer != NULL) {
			// Create escaped version of the GString that represents the model file
			gchar *model_content_escaped = g_strescape(buffer, NULL);
			// Create message in the (json)syntax that the verification server wants
			g_string_append_printf(destination, "{%s,", msg_type_start_sig);
			g_string_append_printf(destination, "%s:\"%s\"}", msg_type_start_sctx_field_key,
			                       model_content_escaped);
			free(model_content_escaped);
			free(buffer);
			parse_success = true;
		}
	}
	return parse_success;
}


/**
 * @brief Given the model at the file path in ws_data_ptr->file_path, builds
 * a verification request message and sends it via the ws_connection. 
 * If building the message to send fails, the content of ws_data_ptr is updated accordingly.
 * 
 * @param ws_connection Connection via which to send the message
 * @param ws_data_ptr (inout) data for this verification attempt to build the request from and
 * update based on success/non-success.
 */
void send_verif_req_message(struct mg_connection *ws_connection, ws_verif_data* ws_data_ptr) {
	if (ws_connection == NULL || ws_data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Send verification request message - "
		              "invalid parameters");
		return;
	}
	
	// Read sctx model from file, build msg with necessary formatting
	GString *g_verif_msg_str = g_string_new("");
	bool parse_success = parse_model_into_verif_msg_str(g_verif_msg_str, ws_data_ptr->file_path->str);
	
	// Check if parsing succeeded
	if (!parse_success) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Send verification request message - "
		              "unable to parse model file");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
		g_string_free(g_verif_msg_str, true);
		return;
	}
	
	// Try to send verification request
	if (g_verif_msg_str != NULL && g_verif_msg_str->len > 0) {
		syslog_server(LOG_INFO, 
		              "Websocket engine uploader: Send verification request message - "
		              "sending request to verifier server");
		ssize_t sent_bytes = mg_ws_send(ws_connection, g_verif_msg_str->str, 
		                                g_verif_msg_str->len, WEBSOCKET_OP_TEXT);
		if (sent_bytes <= 0) {
			syslog_server(LOG_ERR, 
			              "Send verification request message - sending verification request failed");
			ws_data_ptr->finished = true;
			ws_data_ptr->success = false;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Send verification request message - "
		              "failed to construct message to send");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
	}
	g_string_free(g_verif_msg_str, true);
}


/**
 * @brief Updates ws_data_ptr contents based on the contents of the result msg (ws_msg).
 * Assumes that ws_msg contains a message from the verification server of 
 * type ENG_VERIFICATION_REQUEST_RESULT.
 * 
 * @param ws_msg in-param containing the result message from the verification server
 * @param ws_data_ptr inout-param that will be updated with the message content
 */
void process_verification_result_msg(struct mg_ws_message *ws_msg, ws_verif_data *ws_data_ptr) {
	if (ws_msg == NULL || ws_data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Process verification result message - "
		              "invalid parameters");
		return;
	}
	
	// Check for presence of sequence in msg that indicates verification success
	char *verif_success = strstr(ws_msg->data.ptr, msg_type_result_status_true_sig);
	
	if (verif_success) {
		syslog_server(LOG_INFO, 
		              "Websocket engine uploader: Process verification result message - "
		              "engine satisfies all its properties");
		ws_data_ptr->success = true;
		ws_data_ptr->finished = true;
	} else {
		// Verification failed/unsuccessful
		// Differentiate between status:false and unspecified failure
		char *status_false_in_reply = strstr(ws_msg->data.ptr, msg_type_result_status_false_sig);
		if (!status_false_in_reply) {
			// No 'status' field in answer with either true or false
			syslog_server(LOG_INFO, 
			              "Websocket engine uploader: Process verification result message - "
			              "engine verification result inconclusive");
			ws_data_ptr->message  = g_string_new("Engine verification done but result is unknown.");
		} else {
			// Ordinary failure. Save server's reply (to forward to client later on)
			syslog_server(LOG_INFO, 
			              "Websocket engine uploader: Process verification result message - "
			              "engine does not satisfy all its properties");
			ws_data_ptr->message  = g_string_new("");
			g_string_append_printf(ws_data_ptr->message,"%s", ws_msg->data.ptr);
			ws_data_ptr->message_is_json_str = true;
		}
		ws_data_ptr->success = false;
		ws_data_ptr->finished = true;
	}
}


/**
 * @brief Process a reply message from a verification server. Depending on the contents 
 * of the message (ws_msg), updates ws_verif_data contents.
 * 
 * @param ws_msg The message received from the verifications server
 * @param ws_data_ptr (inout-param) The verification data struct to be updated acc. to server msg.
 */
void process_verif_server_reply(struct mg_ws_message *ws_msg, ws_verif_data *ws_data_ptr) {
	// Parses mg_ws_message, which is expected to contain a message from the verification server.
	// Then adjusts ws_data_ptr struct according to server's message.
	if (ws_msg == NULL || ws_data_ptr == NULL) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Process verification server reply - "
		              "invalid parameters");
		return;
	}
	
	// Check that expected field "__MESSAGE_TYPE__" is contained in message
	char *msg_type_is_defined = strstr(ws_msg->data.ptr, msg_type_field_key);
	if (!msg_type_is_defined) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Process verification server reply - "
		              "reply lacks __MESSAGE_TYPE__ field");
		return;
	}
	
	// Search for occurrence of msg_type_received_sig and msg_type_result_sig respectively
	char *type_verif_req_received = strstr(ws_msg->data.ptr, msg_type_received_sig);
	char *type_verif_req_result = strstr(ws_msg->data.ptr, msg_type_result_sig);
	
	if (type_verif_req_received) {
		// verification has started
		syslog_server(LOG_INFO, 
		              "Websocket engine uploader: Process verification server reply - "
		              "verification server begun verification");
		ws_data_ptr->started = true;
	} else if (type_verif_req_result) {
		syslog_server(LOG_INFO, 
		              "Websocket engine uploader: Process verification server reply - "
		              "verification server completed verification");
		// verification has finished, parse result (updates ws_data_ptr)
		process_verification_result_msg(ws_msg, ws_data_ptr);
	} else {
		// Unknown message type specified by the server.
		syslog_server(LOG_WARNING, 
		              "Websocket engine uploader: Process verification server reply - "
		              "invalid reply format");
		// We are pessimistic and assume that the verification server will not reply again
		// after this "mistake"
		ws_data_ptr->success = false;
		ws_data_ptr->finished = true;
	}
}


/**
 * @brief Function destined to be used as the callback for when
 * an event occurs on a websocket connection to a verifier server.
 * Depending on the event, updates the ws_verif_data associated with this connection 
 * according to the event that occurred.
 * 
 * @param ws_connection websocket connection to the verifier server
 * @param ev event code specifying the type of event/"something" that has occurred
 * @param ev_data event data
 * @param fn_data ws_verif_data ptr specified for this connection
 */
void websocket_verification_callback(struct mg_connection *ws_connection, 
                                     int ev, void *ev_data, void *fn_data) {
	ws_verif_data *ws_data_ptr = fn_data;
	if (ws_connection == NULL) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: verification callback - connection is NULL");
		// Error is encountered, the verification can't be completed 
		ws_data_ptr->success = false;
		ws_data_ptr->finished = true;
	} else if (ev == MG_EV_ERROR) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: verification callback - received error event: %s", 
		              (char *) ev_data);
		// Error is encountered, the verification can't be completed 
		ws_data_ptr->success = false;
		ws_data_ptr->finished = true;
	} else if (ev == MG_EV_WS_OPEN) {
		// The websocket connection is open, send the verification request to the server
		send_verif_req_message(ws_connection, ws_data_ptr);
	} else if (ev == MG_EV_WS_MSG) {
		// "Normal" message -> Process message from server
		process_verif_server_reply((struct mg_ws_message *) ev_data, ws_data_ptr);
	} else if (ev == MG_EV_CLOSE) {
		syslog_server(LOG_INFO, 
		              "Websocket engine uploader: verification callback - "
		              "closing websocket connection to verifier server");
		ws_data_ptr->finished = true;
	}
}


verif_result verify_engine_model(const char* f_filepath) {
	struct mg_mgr event_manager;
	ws_verif_data ws_verif_data = {false, false, false, false, g_string_new(f_filepath), NULL};
	
	if (verifier_url == NULL) {
		syslog_server(LOG_ERR, 
		              "Websocket engine uploader: Verify engine model - "
		              "no verifier URL has been set, abort");
		verif_result result_data;
		result_data.success = false;
		result_data.message_is_json_str = false;
		result_data.message = g_string_new("No verifier server URL has been set, "
		                                   "thus no verification was possible");
		return result_data;
	}
	
	// Client connection, init event manager
	struct mg_connection *ws_connection;
	mg_mgr_init(&event_manager);
	
	// Create client; callback will automatically initiate and process verification
	ws_connection = mg_ws_connect(&event_manager, verifier_url, websocket_verification_callback, 
	                              &ws_verif_data, NULL);
	
	unsigned int poll_counter = 0;
	// Wait for Verification to end
	while (ws_connection && ws_verif_data.finished == false) {
		// Wait/Poll for reply
		mg_mgr_poll(&event_manager, websocket_single_poll_length_ms);
		++poll_counter;
		
		// Check if verification has started yet; abort if not started within certain time.
		if (poll_counter > websocket_max_polls_before_start && !ws_verif_data.started) { 
			ws_verif_data.finished = true;
			ws_verif_data.success = false;
			syslog_server(LOG_WARNING, 
			              "Websocket engine uploader: Verify engine model - "
			              "verification did not start within %u ms, abort", 
			              (poll_counter * websocket_single_poll_length_ms));
		}
	}
	
	if (ws_connection && !ws_connection->is_closing) {
		// Close if not yet closing. Then one poll, otherwise will not close.
		// '1' is length of payload ("0"), which indicates reason for closing.
		mg_ws_send(ws_connection, "0", 1, WEBSOCKET_OP_CLOSE);
		mg_mgr_poll(&event_manager, 1000);
	}
	
	// Free event manager resources, including ws_connection
	mg_mgr_free(&event_manager);
	verif_result result_data;
	result_data.success = ws_verif_data.success;
	result_data.message_is_json_str = ws_verif_data.message_is_json_str;
	result_data.message = ws_verif_data.message;
	
	// Free string allocated for filepath of model file
	g_string_free(ws_verif_data.file_path, true);
	return result_data;
}


void set_verifier_url(const char *upd_verifier_url) {
	if (upd_verifier_url == NULL) {
		syslog_server(LOG_WARNING, "Set verifier URL - proposed URL is NULL, URL not updated");
		return;
	}
	if (verifier_url != NULL) {
		free(verifier_url);
		verifier_url = NULL;
	}
	verifier_url = strdup(upd_verifier_url);
	syslog_server(LOG_NOTICE, "Set verifier URL - verifier URL set to: %s", verifier_url);
}


const char * get_verifier_url() {
	return verifier_url;
}


void free_verifier_url() {
	if (verifier_url != NULL) {
		free(verifier_url);
		verifier_url = NULL;
	}
}


void load_cached_verifier_url() {
	// load verifier url from cache file
	FILE *file = fopen(cache_file_verifier_url, "r");
	if (file) {
		// Adapted from https://stackoverflow.com/a/174743
		// Read entire file into buffer
		char *buffer = NULL;
		size_t length = 0;
		ssize_t bytes_read_count = getdelim(&buffer, &length, '\0', file);
		if (bytes_read_count != -1 && buffer != NULL) {
			// url read, update verifier_url accordingly
			free_verifier_url();
			verifier_url = strdup(buffer);
			// Ensure no newline at the end, necessary on e.g. Raspberry Pi OS (but not Ubuntu 22.04)
			int len = strlen(verifier_url);
			if (len > 0 && verifier_url[len-1] == '\n') {
				verifier_url[len-1] = '\0';
			}
			
			free(buffer);
			syslog_server(LOG_INFO, 
			              "Load cached verifier URL - loaded URL %s from cache", 
			              verifier_url);
		} else {
			syslog_server(LOG_NOTICE, "Load cached verifier URL - no content in cache file");
		}
	} else {
		syslog_server(LOG_NOTICE, 
		              "Load cached verifier URL - cache file could not be opened");
	}
}


void cache_verifier_url() {
	// write current verifier url to cache file unless url is null
	if (verifier_url == NULL) {
		syslog_server(LOG_NOTICE, "Cache verifier URL - not cached as URL is NULL");
		return;
	}
	
	// Open file, write string into file, close file.
	FILE* file = fopen(cache_file_verifier_url, "w");
	if (file == NULL) {
		syslog_server(LOG_ERR, "Cache verifier URL - cache file opening failed");
		return;
	}
	
	// Write the content to the file
	fputs(verifier_url, file);
	syslog_server(LOG_INFO, "Cache verifier URL - cached URL %s", verifier_url);
	
	// Close the file
	fclose(file);
}
