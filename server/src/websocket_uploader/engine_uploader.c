/*
 *
 * Copyright (C) 2022 University of Bamberg, Software Technologies Research Group
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
#include <glib.h>
#include "../server.h"

typedef struct {
	bool started;
	bool finished;
	bool success;
	GString* file_path;
	GString* srv_result_full_msg;
} ws_verif_data;

static const char verifier_url[] = "ws://141.13.106.29:8080/engineverification/";
// For development, local verification server.
//static const char verifier_url[] = "ws://127.0.0.1:8080/engineverification/";

void process_verif_server_reply(struct mg_connection *c, struct mg_ws_message *wm, ws_verif_data* ws_data_ptr);

void send_verif_req_message(struct mg_connection *c, ws_verif_data* ws_data_ptr);

void websock_verification_callback(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

static const unsigned int websocket_single_poll_length_ms = 250;
static const unsigned int websocket_max_polls_before_start = 60;

void send_verif_req_message(struct mg_connection *c, ws_verif_data* ws_data_ptr) {
	// Read sctx model from file and send that as the message. https://stackoverflow.com/a/174743
	// Open the file with the sctx model
	FILE* fp = fopen((*ws_data_ptr).file_path->str, "r");
	GString* g_fullmsg = g_string_new("");
	if (fp) {
		char* buffer = NULL;
		size_t length = 0;
		ssize_t bytes_read_count = getdelim(&buffer, &length, '\0', fp);
		if (bytes_read_count != -1 && buffer != NULL) {
			//- Create escaped version of the GString that represents the model file
			gchar* g_c_escaped = g_strescape(buffer, NULL);
			//- Create message in the (json)syntax that the verification server wants
			g_string_append_printf(g_fullmsg, "{\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_START\",");
			g_string_append_printf(g_fullmsg, "\"sctx\":\"%s\"}", g_c_escaped);
			free(g_c_escaped);
			free(buffer);
		} else {
			syslog_server(LOG_ERR, "Upload Engine - Websocket callback could not load the model file");
			ws_data_ptr->finished = true;
			ws_data_ptr->success = false;
		}
	} else {
		syslog_server(LOG_ERR, "Upload Engine - Unable to open engine model file to be uploaded");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
	}
	// Send verification request
	if(g_fullmsg != NULL && g_fullmsg->len > 0){
		syslog_server(LOG_INFO, "Upload Engine - Now sending verification request to swtbahn-verifier");
		mg_ws_send(c, g_fullmsg->str, g_fullmsg->len, WEBSOCKET_OP_TEXT);
	} else {
		syslog_server(LOG_ERR, "Upload Engine - Websocket msg payload setup has gone wrong");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
	}
	g_string_free(g_fullmsg, true);
}

void process_verif_server_reply(struct mg_connection *c, struct mg_ws_message *wm, ws_verif_data* ws_data_ptr) {
	// Parses mg_ws_message, which is expected to contain a message from the verification server.
	// Then adjusts ws_data_ptr struct according to server's message.
	
	char* msg_type_is_defined = strstr(wm->data.ptr, "__MESSAGE_TYPE__");
	if (!msg_type_is_defined) {
		// Message type field not specified, stop.
		syslog_server(LOG_WARNING, "Upload Engine - Verification Server replied in unknown format");
		return;
	}
	
	char* type_verif_req_rec = strstr(wm->data.ptr, "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RECEIVED\"");
	char* type_verif_req_result = strstr(wm->data.ptr, "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RESULT\"");
	
	if (type_verif_req_rec) {
		syslog_server(LOG_NOTICE, "Upload Engine - Verification Server started verification");
		ws_data_ptr->started = true;
	} else if (type_verif_req_result) {
		// Parse result
		char* verif_success = strstr(wm->data.ptr, "\"status\":true");
		if (verif_success) {
			syslog_server(LOG_NOTICE, "Upload Engine - Verification Server finished,"
			                          " verification succeeded");
			ws_data_ptr->success = true;
			ws_data_ptr->finished = true;
		} else {
			// Verification failed/unsuccessful
			ws_data_ptr->success = false;
			ws_data_ptr->finished = true;
			// Differentiate between status:false and unspecified failure
			bool status_false_in_reply = (strstr(wm->data.ptr, "\"status\":false") != NULL);
			if (!status_false_in_reply){
				// No 'status' field in answer with either true or false
				syslog_server(LOG_WARNING, "Upload Engine - Verification Server finished,"
													" verification status not specified");
			} else {
				// Ordinary failure
				// Save server's reply (to forward to client later on)
				syslog_server(LOG_NOTICE, "Upload Engine - Verification Server finished, "
				              "verification did not succeed");
				ws_data_ptr->srv_result_full_msg  = g_string_new("");
				g_string_append_printf(ws_data_ptr->srv_result_full_msg,"%s",wm->data.ptr);
			}
		}
	} else {
		// Unknown message type specified by the server.
		syslog_server(LOG_WARNING, "Upload Engine - Verification Server replied in unknown msg format");
	}
}


void websock_verification_callback(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	ws_verif_data* ws_data_ptr = fn_data;
	if (ev == MG_EV_ERROR) {
		syslog_server(LOG_ERR, "Upload Engine - Websocket callback encountered error: %s", (char *) ev_data);
		// Error is encountered, the verification can't be completed -> success false; finished true.
		ws_data_ptr->success = false;
		ws_data_ptr->finished = true;
	} else if (ev == MG_EV_WS_OPEN) {
		// The websocket connection is opened, send the verification request to the server
		send_verif_req_message(c, ws_data_ptr);
	} else if (ev == MG_EV_WS_MSG) {
		// Receive & Process message from server
		process_verif_server_reply(c, (struct mg_ws_message *) ev_data, ws_data_ptr);
	} else if (ev == MG_EV_CLOSE) {
		syslog_server(LOG_INFO, "Upload Engine - Now closing websocket connection to swtbahn-verifier");
		ws_data_ptr->finished = true;
	}
}

verif_result verify_engine_model(const char* f_filepath) {
	struct mg_mgr mgr;
	ws_verif_data ws_verifData;
	ws_verifData.started 	= false;
	ws_verifData.finished 	= false;
	ws_verifData.success 	= false;
	ws_verifData.file_path 	= g_string_new(f_filepath);
	ws_verifData.srv_result_full_msg = NULL;
	
	// Client connection
	struct mg_connection *c;
	
	// Initialise event manager
	mg_mgr_init(&mgr);
	
	// Create client; callback will automatically initiate and process verification
	c = mg_ws_connect(&mgr, verifier_url, websock_verification_callback, &ws_verifData, NULL);
	
	unsigned int poll_counter = 0;
	
	// Wait for Verification to end
	while (c && ws_verifData.finished == false) {
		mg_mgr_poll(&mgr, websocket_single_poll_length_ms); // Wait for reply
		++poll_counter;
		// Check if verification has started yet; abort if not started within certain time.
		if (poll_counter > websocket_max_polls_before_start && !ws_verifData.started){ 
			//poll_counter *  milliseconds passed without verification start
			ws_verifData.finished = true;
			ws_verifData.success = false;
			syslog_server(LOG_WARNING, 
							"Upload Engine - Verification did not start within %d ms, abort", 
							(poll_counter * websocket_single_poll_length_ms));
		}
	}
	
	if(c && !c->is_closing){
		// If connection is not yet closing, send close. After that, one more event poll has to be performed,
		// otherwise the close will not complete. (Also, buf = reason = max length of 1)
		mg_ws_send(c, "0", 1, WEBSOCKET_OP_CLOSE);
		mg_mgr_poll(&mgr, 1000);
	}
	// Free event manager resources, including connection c
	mg_mgr_free(&mgr);
	verif_result result_data;
	result_data.success = ws_verifData.success;
	result_data.srv_result_full_msg = ws_verifData.srv_result_full_msg;
	// Free string allocated for filepath of model file
	g_string_free(ws_verifData.file_path, true);
	return result_data;
}