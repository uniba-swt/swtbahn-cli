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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 * - Eugene Yip <https://github.com/eyip002>
 *
 */
 

#include <onion/shortcuts.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "handler_upload.h"
#include "server.h"
#include "dynlib.h"
#include "dyn_containers_interface.h"
#include "websocket_uploader/engine_uploader.h"
#include "communication_utils.h"

typedef onion_connection_status o_con_status;

static const char engine_dir[] = "engines";
static const char engine_extensions[][5] = { "c", "h", "sctx" };
static const int engine_extensions_count = 3;

static const char interlocker_dir[] = "interlockers";
static const char interlocker_extensions[][5] = { "bahn" };
static const int interlocker_extensions_count = 1;

extern pthread_mutex_t dyn_containers_mutex;

static bool clear_dir(const char dir[]) {
	int result = 0;
	
	DIR *dir_handle = opendir(dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, "Clear Directory - Directory %s could not be opened", dir);
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

static void remove_file_extension(char filepath_destination[], const char filepath_source[], 
                                  const char extension[]) {
	strcpy(filepath_destination, filepath_source);
	size_t filepath_len = strlen(filepath_source);
	size_t extension_len = strlen(extension);
	if (filepath_len > extension_len) {
		filepath_destination[filepath_len - extension_len] = '\0';
	}
}

static bool engine_file_exists(const char filename[]) {
	DIR *dir_handle = opendir(engine_dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, 
		              "Engine file exists check - directory %s could not be opened", 
		              engine_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_NOTICE, 
			              "Engine file exists check - engine %s already exists", 
			              filename);
			return true;
		}
	}
	closedir(dir_handle);
	return false;
}

static bool remove_engine_files(const char library_name[]) {
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

static bool plugin_is_unremovable(const char name[]) {
	return (strstr(name, "(unremovable)") != NULL);
}


o_con_status handler_upload_engine(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		
		if (handle_param_miss_check(res, "Upload engine", "file(name)", filename)) {
			return OCS_PROCESSED;
		} else if (temp_filepath == NULL) {
			// Either something went wrong with the fs(?), or the file was not attached at all.
			send_common_feedback(res, HTTP_BAD_REQUEST, "engine file is invalid or missing");
			syslog_server(LOG_ERR, "Request: Upload engine - engine file is invalid or missing");
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Upload engine - engine file: %s - start", filename);
		
 		if (engine_file_exists(filename)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "engine file already exists");
			syslog_server(LOG_ERR, 
			              "Request: Upload engine - engine file: %s - "
			              "engine file already exists - abort", 
			              filename);
			return OCS_PROCESSED;
		}
		
		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".sctx");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "lib%s", filename_noextension);
		
		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", engine_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_DEBUG, 
		              "Request: Upload engine - engine file: %s - copied engine file from %s to %s", 
		              filename, temp_filepath, final_filepath);
		
		if (verification_enabled) {
			verif_result engine_verif_result = verify_engine_model(final_filepath);
			if (!engine_verif_result.success) {
				// Stop upload if verification did not succeed
				syslog_server(LOG_NOTICE, "Request: Upload Engine - engine verification failed - abort");
				remove_engine_files(libname);
				if (engine_verif_result.message != NULL) {
					send_common_feedback(res, HTTP_BAD_REQUEST, engine_verif_result.message->str);
					g_string_free(engine_verif_result.message, true);
				} else {
					send_common_feedback(res, HTTP_BAD_REQUEST, 
					                     "verification failed due to unknown reason");
				}
				return OCS_PROCESSED;
			}
		}
		
		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".sctx");
		const dynlib_status status = dynlib_compile_scchart(filepath, engine_dir);
		if (status == DYNLIB_COMPILE_SCCHARTS_C_ERR || status == DYNLIB_COMPILE_SHARED_SCCHARTS_ERR) {
			remove_engine_files(libname);
			
			send_common_feedback(res, HTTP_INTERNAL_ERROR, "engine file could not be compiled");
			syslog_server(LOG_ERR, 
			              "Request: Upload engine - engine file: %s - could not be "
			              "compiled into a C file and then to a shared library - abort", 
			              filepath);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_DEBUG, 
		              "Request: Upload engine - engine file: %s - engine compiled",
		              filename);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_free_engine_slot();
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_engine_files(libname);
			
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "No available engine slot");
			syslog_server(LOG_WARNING, 
			              "Request: Upload engine - engine file: %s - "
			              "no available engine slot - abort", 
			              filename);
			return OCS_PROCESSED;
		}
		
		snprintf(filepath, sizeof(filepath), "%s/%s", engine_dir, libname);
		dyn_containers_set_engine(engine_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		send_common_feedback(res, HTTP_OK, "");
		syslog_server(LOG_NOTICE, "Request: Upload engine - engine file: %s - finish", filename);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Upload engine");
	}
}

o_con_status handler_remove_engine(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "engine-name");
		
		if (handle_param_miss_check(res, "Remove engine", "engine-name", name)) {
			return OCS_PROCESSED;
		} else if (plugin_is_unremovable(name)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "engine to remove is unremovable");
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine to remove (%s) is unremovable", 
			              name);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine: %s - start", name);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_engine_slot(name);
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			send_common_feedback(res, HTTP_NOT_FOUND, "Engine could not be found");
			syslog_server(LOG_WARNING, 
			              "Request: Remove engine - engine: %s - engine could not be found - abort", 
			              name);
			return OCS_PROCESSED;
		}
		
		const bool engine_freed_successfully = dyn_containers_free_engine(engine_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!engine_freed_successfully) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "Engine still in use");
			syslog_server(LOG_WARNING, 
			              "Request: Remove engine - engine: %s - engine is still in use - abort", 
			              name);
			return OCS_PROCESSED;
		}
		
		if (!remove_engine_files(name)) {
			syslog_server(LOG_WARNING, 
			              "Request: Remove engine - engine: %s - files could not be removed", 
			              name);
		}
		send_common_feedback(res, HTTP_OK, "");
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine: %s - finish", name);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Remove engine");
	}
}

static bool interlocker_file_exists(const char filename[]) {
	DIR *dir_handle = opendir(interlocker_dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, 
		              "Interlocker file exists check - directory %s could not be opened", 
		              interlocker_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			return true;
		}
	}
	closedir(dir_handle);
	return false;
}

static bool remove_interlocker_files(const char library_name[]) {
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

o_con_status handler_upload_interlocker(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		
		if (handle_param_miss_check(res, "Upload interlocker", "file(name)", filename)) {
			return OCS_PROCESSED;
		} else if (temp_filepath == NULL) {
			// Either something went wrong with the fs(?), or the file was not attached at all.
			send_common_feedback(res, HTTP_BAD_REQUEST, "interlocker file is invalid or missing");
			syslog_server(LOG_ERR, 
			              "Request: Upload interlocker - interlocker file is invalid or missing");
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, 
		              "Request: Upload interlocker - interlocker file: %s - start", 
		              filename);
		
		if (interlocker_file_exists(filename)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "interlocker file already exists");
			syslog_server(LOG_ERR, 
			              "Request: Upload interlocker - interlocker file: %s - "
			              "file already exists - abort", 
			              filename);
			return OCS_PROCESSED;
		}
		
		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".bahn");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "libinterlocker_%s", filename_noextension);
		
		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", interlocker_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_DEBUG, 
		              "Request: Upload interlocker - interlocker file: %s - "
		              "copied interlocker BahnDSL file from %s to %s",
		              filename, temp_filepath, final_filepath);
		
		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".bahn");
		const dynlib_status status = dynlib_compile_bahndsl(filepath, interlocker_dir);
		if (status == DYNLIB_COMPILE_SHARED_BAHNDSL_ERR) {
			send_common_feedback(res, HTTP_INTERNAL_ERROR, "interlocker file could not be compiled");
			syslog_server(LOG_ERR, 
			              "Request: Upload interlocker - interlocker file: %s - "
			              "interlocker could not be compiled - abort", 
			              filename);
			remove_interlocker_files(libname);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_DEBUG, 
		              "Request: Upload interlocker - interlocker file: %s - interlocker compiled", 
		              filename);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_free_interlocker_slot();
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_interlocker_files(libname);
			
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "no interlocker slot available");
			syslog_server(LOG_WARNING, 
			              "Request: Upload interlocker - interlocker file: %s - "
			              "no available interlocker slot - abort", 
			              filename);
			return OCS_PROCESSED;
		}
		
		snprintf(filepath, sizeof(filepath), "%s/%s", interlocker_dir, libname);
		dyn_containers_set_interlocker(interlocker_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		send_common_feedback(res, HTTP_OK, "");
		syslog_server(LOG_NOTICE, 
		              "Request: Upload interlocker - interlocker file: %s - finish",
		              filename);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Upload interlocker");
	}
}

o_con_status handler_remove_interlocker(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "interlocker-name");
		
		if (handle_param_miss_check(res, "Remove interlocker", "interlocker-name", name)) {
			return OCS_PROCESSED;
		} else if (plugin_is_unremovable(name)) {
			send_common_feedback(res, HTTP_BAD_REQUEST, "interlocker to remove is unremovable");
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker to remove (%s) is unremovable", 
			              name);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker: %s - start", name);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_interlocker_slot(name);
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			send_common_feedback(res, HTTP_NOT_FOUND, "Interlocker to remove could not be found");
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker: %s - "
			              "interlocker could not be found - abort", 
			              name);
			return OCS_PROCESSED;
		}
		
		const bool free_success = dyn_containers_free_interlocker(interlocker_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!free_success) {
			send_common_feedback(res, CUSTOM_HTTP_CODE_CONFLICT, "Interlocker is still in use");
			syslog_server(LOG_WARNING, 
			              "Request: Remove interlocker - interlocker: %s - "
			              "interlocker is still in use - abort", 
			              name);
			return OCS_PROCESSED;
		}
		
		if (!remove_interlocker_files(name)) {
			syslog_server(LOG_WARNING, 
			              "Request: Remove interlocker - interlocker: %s - "
			              "files could not be removed", 
			              name);
		}
		send_common_feedback(res, HTTP_OK, "");
		syslog_server(LOG_NOTICE, 
		              "Request: Remove interlocker - interlocker: %s - finish",
		              name);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Remove interlocker");
	}
}
