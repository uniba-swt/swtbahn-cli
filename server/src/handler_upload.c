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

#include "server.h"
#include "dynlib.h"
#include "dyn_containers_interface.h"
#include "mongoose.h"

static const char engine_dir[] = "engines";
static const char engine_extensions[][5] = { "c", "h", "sctx" };
static const int engine_extensions_count = 3;

static const char interlocker_dir[] = "interlockers";
static const char interlocker_extensions[][5] = { "bahn" };
static const int interlocker_extensions_count = 1;

static const char verifier_url[] = "ws://141.13.106.29:8080/engineverification/";

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

bool engine_is_unremovable(const char name[]) {
	bool result = strcmp(name, "libtrain_engine_default (unremovable)") == 0
	              || strcmp(name, "libtrain_engine_linear (unremovable)") == 0;
	return result;
}

struct ws_verif_data {
	bool finished;
	bool success;
	GString* file_path;
	//Can also pass the verifier return message/logs over this struct in the future
};


// Print websocket response and signal that we're done
static void websock_verification_callback(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	if (ev == MG_EV_ERROR) {
		// On error, log error message
		syslog_server(LOG_ERR, "Request: Upload - Websocket callback encountered error: %s", (char *) ev_data);
	} else if (ev == MG_EV_WS_OPEN) {
		// Read sctx model from file and send that as the message.
		//https://stackoverflow.com/a/174743
		char* buffer = NULL;
		size_t length;
		FILE* fp = fopen((*(struct ws_verif_data *) fn_data).file_path->str, "r");
		if(fp){
			ssize_t bytes_read_count = getdelim( &buffer, &length, '\0', fp);
			if (bytes_read_count != -1){
				//- Create escaped version of the GString that represents the model file
				//- Create message in the (json)syntax that the verification server wants
				gchar* g_c_escaped = g_strescape(buffer, NULL);
				GString* g_fullmsg = g_string_new("");
				g_string_append_printf(g_fullmsg, "{\"sctx\":\"%s\"}", g_c_escaped);
				free(g_c_escaped);
				
				//Send verification request
				if(g_fullmsg != NULL){
					//printf("Sending verification request\n");
					syslog_server(LOG_INFO, "Request: Upload - Now sending verification request to swtbahn-verifier");
					mg_ws_send(c, g_fullmsg->str, g_fullmsg->len, WEBSOCKET_OP_TEXT);
				} else {
					syslog_server(LOG_ERR, "Request: Upload - Websocket msg payload setup has gone wrong");
				}
				g_string_free(g_fullmsg, true);
				free(buffer);
			} else {
				syslog_server(LOG_ERR, "Request: Upload - Websocket callback could not load the model file");
			}
		}
		
	} else if (ev == MG_EV_WS_MSG) {
		// Got Response
		struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
		
		//Check if the response contains that status field -> if yes, it's the verification result
		char* isResult = strstr(wm->data.ptr, "\"status\":");
		printf("Received: %s\n", wm->data.ptr);
		if (isResult){	
			//Check how the verification went
			char* ret = strstr(wm->data.ptr, "\"status\":true");
			printf("%s",wm->data.ptr);
			if(ret){
				printf("\nVerif: Good Result\n");
				syslog_server(LOG_INFO, "Request: Upload - Verification successful (result positive)");
				(*(struct ws_verif_data *) fn_data).success = true;
			} else {
				printf("\nVerif: Negative Result\n");
				syslog_server(LOG_INFO, "Request: Upload - Verification not successful (result negative)");
				(*(struct ws_verif_data *) fn_data).success = false;
			}
			(*(struct ws_verif_data *) fn_data).finished = true;
		}
	}
	if (ev == MG_EV_ERROR){
		//If an error is encountered, the verification can't be completed -> success false; finished true.
		(*(struct ws_verif_data *) fn_data).success = false;
		(*(struct ws_verif_data *) fn_data).finished = true;
	} else if (ev == MG_EV_CLOSE) {
		syslog_server(LOG_INFO, "Request: Upload - Now closing websocket connection to swtbahn-verifier");
		(*(struct ws_verif_data *) fn_data).finished = true;
	}
}

onion_connection_status handler_upload_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	printf("1 - handler_upload_engine\n");
	if (/*running &&*/ ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		printf("2 - handler_upload_engine\n");
		if (filename == NULL || temp_filepath == NULL) {
			syslog_server(LOG_ERR, "Request: Upload - engine file is invalid");
			
			onion_response_printf(res, "Engine file is invalid");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		printf("3 - handler_upload_engine\n");
 		if (engine_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload - engine file already exists");
			
			onion_response_printf(res, "Engine file already exists");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}  
		printf("4 - handler_upload_engine\n");
		
		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".sctx");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "lib%s", filename_noextension);

		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", engine_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_NOTICE, "Request: Upload - copied engine SCCharts file from %s to %s", 
					  temp_filepath, final_filepath);

		//BLuedtke: Now try to contact verification server and verify engine before proceeding
		// Should we try to compile the model first? -> discuss
		// Has to be done over websocket connection.
		printf("5 - handler_upload_engine\n");
		if(verification_enabled){
			struct mg_mgr mgr;
			struct ws_verif_data ws_verifData;
			ws_verifData.file_path = g_string_new(final_filepath);
			// Client connection
			struct mg_connection *c;
			// Initialise event manager
			mg_mgr_init(&mgr);
			// Create client
			c = mg_ws_connect(&mgr, verifier_url, websock_verification_callback, &ws_verifData, NULL);
			while (c && ws_verifData.finished == false) {
				mg_mgr_poll(&mgr, 1000); // Wait for reply
			}
			if(c && !c->is_closing){
				//If connection is not yet closing, send close. After that, one more event poll has to be performed,
				// otherwise the close msg will not be sent. (Also, buf = reason = max length of 1 apparently??)
				mg_ws_send(c, "0", 1, 1000);
				mg_mgr_poll(&mgr, 1000);
			}
			mg_mgr_free(&mgr); // Deallocate resources
			
			//Free string allocated for loading model file
			g_string_free(ws_verifData.file_path, true);
			printf("6 - handler_upload_engine\n");
			//Stop upload if verification did not succeed
			if(!ws_verifData.success){
				pthread_mutex_unlock(&dyn_containers_mutex);
				syslog_server(LOG_ERR, "Request: Upload - engine verification failed");
				
				onion_response_printf(res, "Engine verification failed");
				onion_response_set_code(res, HTTP_BAD_REQUEST);
				return OCS_PROCESSED;
			}
		}
		printf("7 - handler_upload_engine\n");
		
		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".sctx");
		const dynlib_status status = dynlib_compile_scchart(filepath, engine_dir);
		if (status == DYNLIB_COMPILE_SCCHARTS_C_ERR || status == DYNLIB_COMPILE_SHARED_SCCHARTS_ERR) {
			remove_engine_files(libname);

			syslog_server(LOG_ERR, "Request: Upload - engine file %s could not be compiled "
                                   "into a C file and then to a shared library", filepath);
			
			onion_response_printf(res, "Engine file %s could not be compiled into a C file "
                                       "and then a shared library", filepath);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_NOTICE, "Request: Upload - engine %s compiled", filename);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_free_engine_slot();
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_engine_files(libname);
		
			syslog_server(LOG_ERR, "Request: Upload - no available engine slot");
			
			onion_response_printf(res, "No available engine slot");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		snprintf(filepath, sizeof(filepath), "%s/%s", engine_dir, libname);
		dyn_containers_set_engine(engine_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		return OCS_PROCESSED;			
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
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
		if (name == NULL || engine_is_unremovable(name)) {
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
			syslog_server(LOG_ERR, "Request: Remove engine - engine %s is still in use", 
						  name);

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
		
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine %s removed", 
					  name);
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

bool interlocker_is_unremovable(const char name[]) {
	bool result = strcmp(name, "interlocker_default (unremovable)") == 0;
	return result;
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
		if (name == NULL || interlocker_is_unremovable(name)) {
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

		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker %s removed",
		              name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove interlocker - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}
