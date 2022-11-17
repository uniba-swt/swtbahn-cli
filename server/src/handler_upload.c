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
 * - Ben-Oliver Hosak <https://github.com/hosakb>
 * - Eugene Yip <https://github.com/eyip002>
 *
 */
 

#include <onion/onion.h>
#include <onion/shortcuts.h>
#include <pthread.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>

#include "handler_upload.h"
#include "server.h"
#include "dynlib.h"
#include "dyn_containers_interface.h"

static const char engine_dir[] = "engines";
static const char engine_extensions[][5] = { "c", "h", "sctx" };
static const int engine_extensions_count = 3;

static const char interlocker_dir[] = "interlockers";
static const char interlocker_extensions[][5] = { "bahn" };
static const int interlocker_extensions_count = 1;

static const char verifier_url[] = "ws://141.13.106.29:8080/engineverification/";
//static const char verifier_url[] = "ws://127.0.0.1:8080/engineverification/"; //For development, local verification server.

static const unsigned int websocket_single_poll_length_ms = 250;
static const unsigned int websocket_max_polls_before_start = 60;

extern pthread_mutex_t dyn_containers_mutex;


bool clear_dir(const char dir[]) {
	int result = 0;
	
	DIR *dir_handle = opendir(dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, "Upload: Directory %s could not be opened", dir);
		return false;
	}
	
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
			continue;
		}
		
		char filepath[PATH_MAX + NAME_MAX];
		sprintf(filepath, "%s/%s", dir, dir_entry->d_name);
		result += remove(filepath);
	}
	
	closedir(dir_handle);
	return (result == 0);
}

bool clear_engine_dir(void) {
	return clear_dir(engine_dir);
}

bool clear_interlocker_dir(void) {
	return clear_dir(interlocker_dir);
}

void remove_file_extension(char filepath_destination[], 
                           const char filepath_source[], const char extension[]) {
	strcpy(filepath_destination, filepath_source);
    size_t filepath_len = strlen(filepath_source);
    size_t extension_len = strlen(extension);
    filepath_destination[filepath_len - extension_len] = '\0';
}

bool engine_file_exists(const char filename[]) {
	DIR *dir_handle = opendir(engine_dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, "Upload: Directory %s could not be opened", engine_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_ERR, "Upload: Engine %s already exists", filename);
			return true;
		}
	}
	closedir(dir_handle);
	return false;
}

bool remove_engine_files(const char library_name[]) {
	// Remove the prefix "lib"
	char name[PATH_MAX + NAME_MAX];
	strcpy(name, library_name + 3);
	
	int result = 0;
	char filepath[PATH_MAX + NAME_MAX];
	for (int i = 0; i < engine_extensions_count; i++) {
		sprintf(filepath, "%s/%s.%s", engine_dir, name, engine_extensions[i]);
		result = remove(filepath);
	}
	sprintf(filepath, "%s/lib%s.so", engine_dir, name);
	result += remove(filepath);
	
	return (result == 0);
} 

bool plugin_is_unremovable(const char name[]) {
	return (strstr(name, "(unremovable)") != NULL);
}

void send_verif_req_message(struct mg_connection *c, ws_verif_data* ws_data_ptr) {
	// Read sctx model from file and send that as the message. https://stackoverflow.com/a/174743
	char* buffer = NULL;
	size_t length;
	FILE* fp = fopen((*ws_data_ptr).file_path->str, "r");
	GString* g_fullmsg = g_string_new("");
	if (fp) {
		ssize_t bytes_read_count = getdelim(&buffer, &length, '\0', fp);
		if (bytes_read_count != -1) {
			//- Create escaped version of the GString that represents the model file
			//- Create message in the (json)syntax that the verification server wants
			gchar* g_c_escaped = g_strescape(buffer, NULL);
			g_string_append_printf(g_fullmsg, "{\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_START\",");
			g_string_append_printf(g_fullmsg, "\"sctx\":\"%s\"}", g_c_escaped);
			free(g_c_escaped);
			free(buffer);
		} else {
			syslog_server(LOG_ERR, "Request: Upload - Websocket callback could not load the model file");
			ws_data_ptr->finished = true;
			ws_data_ptr->success = false;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Upload - Unable to open engine model file to be uploaded");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
	}
	//Send verification request
	if(g_fullmsg != NULL && g_fullmsg->len > 0){
		syslog_server(LOG_INFO, "Request: Upload - Now sending verification request to swtbahn-verifier");
		mg_ws_send(c, g_fullmsg->str, g_fullmsg->len, WEBSOCKET_OP_TEXT);
	} else {
		syslog_server(LOG_ERR, "Request: Upload - Websocket msg payload setup has gone wrong");
		ws_data_ptr->finished = true;
		ws_data_ptr->success = false;
	}
	g_string_free(g_fullmsg, true);
}

void process_verif_server_reply(struct mg_connection *c, struct mg_ws_message *wm, ws_verif_data* ws_data_ptr) {
	//Parses mg_ws_message, which is expected to contain a message from the verification server.
	//Then adjusts ws_data_ptr struct according to server's message.
	
	char* msg_type_is_defined = strstr(wm->data.ptr, "__MESSAGE_TYPE__");
	if (!msg_type_is_defined) {
		//Message type field not specified, stop.
		syslog_server(LOG_WARNING, "Request: Upload - Verification Server replied in unknown format");
		return;
	}
	
	char* type_verif_req_rec = strstr(wm->data.ptr, "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RECEIVED\"");
	char* type_verif_req_result = strstr(wm->data.ptr, "\"__MESSAGE_TYPE__\":\"ENG_VERIFICATION_REQUEST_RESULT\"");
	
	if (type_verif_req_rec) {
		syslog_server(LOG_NOTICE, "Request: Upload - Verification Server started verification");
		ws_data_ptr->started = true;
	} else if (type_verif_req_result) {
		//Parse result
		char* verif_success = strstr(wm->data.ptr, "\"status\":true");
		if (verif_success) {
			syslog_server(LOG_NOTICE, "Request: Upload - Verification Server finished,"
			                          " verification success");
			ws_data_ptr->success = true;
			ws_data_ptr->finished = true;
		} else {
			// Verification failed/unsuccessful
			ws_data_ptr->success = false;
			ws_data_ptr->finished = true;
			//Differentiate between status:false and unspecified failure
			char* verif_fail = strstr(wm->data.ptr, "\"status\":false");
			if (!verif_fail){
				//No 'status' field in answer. Stop with fail
				syslog_server(LOG_WARNING, "Request: Upload - Verification Server finished,"
													" verification status not specified");
			} else {
				// Ordinary failure
				//Save server's reply (to forward to client lateron)
				syslog_server(LOG_NOTICE, "Request: Upload - Verification Server finished, verification fail");
				ws_data_ptr->srv_result_full_msg  = g_string_new("");
				g_string_append_printf(ws_data_ptr->srv_result_full_msg,"%s",wm->data.ptr);
			}
		}
	} else {
		//Unknown message type specified by the server.
		syslog_server(LOG_WARNING, "Request: Upload - Verification Server replied in unknown msg type");
	}
}


void websock_verification_callback(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	ws_verif_data* ws_data_ptr = fn_data;
	
	if (ev == MG_EV_ERROR) {
		syslog_server(LOG_ERR, "Request: Upload - Websocket callback encountered error: %s", (char *) ev_data);
		//If an error is encountered, the verification can't be completed -> success false; finished true.
		(*ws_data_ptr).success = false;
		(*ws_data_ptr).finished = true;
	} else if (ev == MG_EV_WS_OPEN) {
		send_verif_req_message(c, ws_data_ptr);
	} else if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
		process_verif_server_reply(c, wm, ws_data_ptr);
	} else if (ev == MG_EV_CLOSE) {
		syslog_server(LOG_INFO, "Request: Upload - Now closing websocket connection to swtbahn-verifier");
		(*ws_data_ptr).finished = true;
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
	
	// Create client
	c = mg_ws_connect(&mgr, verifier_url, websock_verification_callback, &ws_verifData, NULL);
	
	unsigned int poll_counter = 0;
	
	// Wait for Verification to end
	while (c && ws_verifData.finished == false) {
		mg_mgr_poll(&mgr, websocket_single_poll_length_ms); // Wait for reply
		
		// Check if verification has started yet
		if (++poll_counter > websocket_max_polls_before_start && !ws_verifData.started){ 
			//poll_counter *  milliseconds passed without verification start
			ws_verifData.finished = true;
			ws_verifData.success = false;
			syslog_server(LOG_WARNING, 
							"Request: Upload - engine verification - Verification did not start within %d ms, abort", 
							(poll_counter * websocket_single_poll_length_ms));
		}
	}
	
	if(c && !c->is_closing){
		//If connection is not yet closing, send close. After that, one more event poll has to be performed,
		// otherwise the close msg will not be sent. (Also, buf = reason = max length of 1 apparently??)
		mg_ws_send(c, "0", 1, 1000);
		mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr); // Deallocate resources
	verif_result result_data;
	result_data.success = ws_verifData.success;
	result_data.srv_result_full_msg = ws_verifData.srv_result_full_msg;
	//Free string allocated for loading model file
	g_string_free(ws_verifData.file_path, true);
	return result_data;
}

onion_connection_status handler_upload_engine(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		
		if (filename == NULL || temp_filepath == NULL) {
			syslog_server(LOG_ERR, "Request: Upload - engine file is invalid");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			onion_response_printf(res, "Engine file is invalid");
			return OCS_PROCESSED;
		}
		
 		if (engine_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload - engine file already exists");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			onion_response_printf(res, "Engine file already exists");
			return OCS_PROCESSED;
		}
		
		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".sctx");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "lib%s", filename_noextension);

		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", engine_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_NOTICE, "Request: Upload - copied engine SCCharts file from %s to %s", 
		              temp_filepath, final_filepath);

		if(verification_enabled){
			verif_result v_result = verify_engine_model(final_filepath);
			if(!v_result.success){
				//Stop upload if verification did not succeed
				syslog_server(LOG_ERR, "Request: Upload - Engine verification failed");
				remove_engine_files(libname);
				onion_response_set_code(res, HTTP_BAD_REQUEST);
				if (v_result.srv_result_full_msg != NULL) {
					onion_response_printf(res, "%s", v_result.srv_result_full_msg->str);
					g_string_free(v_result.srv_result_full_msg, true);
				} else {
					onion_response_printf(res, "Engine Verification failed due to unknown reason.");
				}
				return OCS_PROCESSED;
			}
		}
		
		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".sctx");
		const dynlib_status status = dynlib_compile_scchart(filepath, engine_dir);
		if (status == DYNLIB_COMPILE_SCCHARTS_C_ERR || status == DYNLIB_COMPILE_SHARED_SCCHARTS_ERR) {
			remove_engine_files(libname);

			syslog_server(LOG_ERR, "Request: Upload - engine file %s could not be compiled "
			                       "into a C file and then to a shared library", filepath);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			onion_response_printf(res, "Engine file %s could not be compiled into a C file "
			                           "and then a shared library", filepath);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Upload - engine %s compiled", filename);
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_free_engine_slot();
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_engine_files(libname);
			
			syslog_server(LOG_ERR, "Request: Upload - no available engine slot");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			onion_response_printf(res, "No available engine slot");
			return OCS_PROCESSED;
		}
		
		snprintf(filepath, sizeof(filepath), "%s/%s", engine_dir, libname);
		dyn_containers_set_engine(engine_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		onion_response_printf(res, "System not running or wrong request type");
		return OCS_PROCESSED;
	}
}

onion_connection_status handler_refresh_engines(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *train_engines = dyn_containers_get_train_engines();
		onion_response_printf(res, "%s", train_engines->str);
		g_string_free(train_engines, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Refresh engines - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_remove_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "engine-name");
		if (name == NULL || plugin_is_unremovable(name)) {
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine name is invalid or engine is unremovable", 
			              name);
			onion_response_printf(res, "Engine name is invalid or engine is unremovable");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_engine_slot(name);
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine %s could not be found", 
			              name);
			onion_response_printf(res, "Engine %s could not be found", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		const bool engine_freed_successfully = dyn_containers_free_engine(engine_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!engine_freed_successfully) {
			syslog_server(LOG_ERR, "Request: Remove engine - engine %s is still in use", name);

			onion_response_printf(res, "Engine %s is still in use", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		if (!remove_engine_files(name)) {
			syslog_server(LOG_ERR, "Request: Remove engine - engine %s files could not be removed", 
			              name);
			onion_response_printf(res, "Engine %s files could not be removed", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine %s removed", name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove engine - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}

bool interlocker_file_exists(const char filename[]) {
	DIR *dir_handle = opendir(interlocker_dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, "Upload: Directory %s could not be opened", interlocker_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_ERR, "Upload: Interlocker %s already exists", filename);
			return true;
		}
	}
	closedir(dir_handle);
	return false;
}

bool remove_interlocker_files(const char library_name[]) {
	// Remove the prefix "libinterlocker_"
	char name[PATH_MAX + NAME_MAX];
	strcpy(name, library_name + 15);

	int result = 0;
	char filepath[PATH_MAX + NAME_MAX];
	for (int i = 0; i < interlocker_extensions_count; i++) {
		sprintf(filepath, "%s/%s.%s", interlocker_dir, name, interlocker_extensions[i]);
		result = remove(filepath);
	}
	sprintf(filepath, "%s/libinterlocker_%s.so", interlocker_dir, name);
	result += remove(filepath);

	return (result == 0);
}

onion_connection_status handler_upload_interlocker(void *_, onion_request *req,
                                                   onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		if (filename == NULL || temp_filepath == NULL) {
			syslog_server(LOG_ERR, "Request: Upload - interlocker file is invalid");
			
			onion_response_printf(res, "Interlocker file is invalid");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		if (interlocker_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload - interlocker file already exists");
			
			onion_response_printf(res, "Interlocker file already exists");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".bahn");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "libinterlocker_%s", filename_noextension);

		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", interlocker_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_NOTICE, "Request: Upload - copied interlocker BahnDSL file from %s to %s",
		              temp_filepath, final_filepath);

		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".bahn");
		const dynlib_status status = dynlib_compile_bahndsl(filepath, interlocker_dir);
		if (status == DYNLIB_COMPILE_SHARED_BAHNDSL_ERR) {
			remove_interlocker_files(libname);
		
			syslog_server(LOG_ERR, "Request: Upload - interlocker file %s could not be compiled", filepath);
			
			onion_response_printf(res, "Interlocker file %s could not be compiled", filepath);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_NOTICE, "Request: Upload - interlocker %s compiled", filename);

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_free_interlocker_slot();
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_interlocker_files(libname);

			syslog_server(LOG_ERR, "Request: Upload - no available interlocker slot");
			
			onion_response_printf(res, "No available interlocker slot");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		snprintf(filepath, sizeof(filepath), "%s/%s", interlocker_dir, libname);
		dyn_containers_set_interlocker(interlocker_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}

onion_connection_status handler_refresh_interlockers(void *_, onion_request *req,
                                                     onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *interlockers = dyn_containers_get_interlockers();
		onion_response_printf(res, "%s", interlockers->str);
		g_string_free(interlockers, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Refresh interlockers - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_remove_interlocker(void *_, onion_request *req,
                                                   onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "interlocker-name");
		if (name == NULL || plugin_is_unremovable(name)) {
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker name is invalid or interlocker is unremovable", name);
			
			onion_response_printf(res, "Interlocker name is invalid or interlocker is unremovable");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_interlocker_slot(name);
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s could not be found", 
			              name);
			
			onion_response_printf(res, "Interlocker %s could not be found", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		const bool interlocker_freed_successfully = dyn_containers_free_interlocker(interlocker_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!interlocker_freed_successfully) {
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s is still in use", 
			              name);
			
			onion_response_printf(res, "Interlocker %s is still in use", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		if (!remove_interlocker_files(name)) {
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s files could not be removed", 
			              name);
			
			onion_response_printf(res, "Interlocker %s files could not be removed", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker %s removed", name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove interlocker - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}
