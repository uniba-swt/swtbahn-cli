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
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>

#include "server.h"
#include "dynlib.h"
#include "dyn_containers_interface.h"


static const char engine_dir[] = "engines";
static const char engine_extensions[][5] = { "c", "h", "sctx" };
static const int engine_extensions_count = 3;

static const char interlocker_dir[] = "interlockers";
static const char interlocker_extensions[][5] = { "bahn" };
static const int interlocker_extensions_count = 1;


extern pthread_mutex_t dyn_containers_mutex;


bool clear_dir(const char dir[]) {
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
		syslog_server(LOG_ERR, "Engine file exists check - Directory %s could not be opened", engine_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_NOTICE, "Engine file exists check - Engine %s already exists", filename);
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

onion_connection_status handler_upload_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		if (filename == NULL || temp_filepath == NULL) {
			syslog_server(LOG_ERR, "Request: Upload engine - engine file is invalid");
			
			onion_response_printf(res, "Engine file is invalid");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Upload engine - engine file: %s" , filename);
  
 		if (engine_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload engine - engine file: %s -"
			              " engine file already exists", filename);
			
			onion_response_printf(res, "Engine file already exists");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}  
		
		char filename_noextension[NAME_MAX];
		remove_file_extension(filename_noextension, filename, ".sctx");
		char libname[sizeof(filename_noextension)];
		snprintf(libname, sizeof(libname), "lib%s", filename_noextension);
		
		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", engine_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_DEBUG, "Request: Upload engine - engine file: %s -"
		              " copied engine SCCharts file from %s to %s", 
		              filename, temp_filepath, final_filepath);

		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".sctx");
		const dynlib_status status = dynlib_compile_scchart(filepath, engine_dir);
		if (status == DYNLIB_COMPILE_SCCHARTS_C_ERR || status == DYNLIB_COMPILE_SHARED_SCCHARTS_ERR) {
			remove_engine_files(libname);

			syslog_server(LOG_ERR, "Request: Upload engine - engine file: %s - could not be "
			              "compiled into a C file and then to a shared library", filepath);
			
			onion_response_printf(res, "Engine file %s could not be compiled into a C file "
			                      "and then a shared library", filepath);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_DEBUG, "Request: Upload engine - engine file: %s - engine compiled",
		              filename);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_free_engine_slot();
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_engine_files(libname);
		
			syslog_server(LOG_ERR, "Request: Upload engine - engine file: %s - "
			              "no available engine slot", filename);
			
			onion_response_printf(res, "No available engine slot");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		snprintf(filepath, sizeof(filepath), "%s/%s", engine_dir, libname);
		dyn_containers_set_engine(engine_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		syslog_server(LOG_NOTICE, "Request: Upload engine - engine file: %s - finished", filename);
		return OCS_PROCESSED;			
	} else {
		syslog_server(LOG_ERR, "Request: Upload engine - system not running or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}

// What is being refreshed by this? Shouldn't it just be get_engines?
onion_connection_status handler_refresh_engines(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Refresh engines");
		GString *train_engines = dyn_containers_get_train_engines();
		onion_response_printf(res, "%s", train_engines->str);
		g_string_free(train_engines, true);
		syslog_server(LOG_INFO, "Request: Refresh engines - finished");
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
			              "Request: Remove engine - engine name is invalid or engine is unremovable");
						  
			onion_response_printf(res, "Engine name is invalid or engine is unremovable");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine: %s", name);
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_engine_slot(name);
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Remove engine - engine: %s - engine could not be found", 
						  name);
						  
			onion_response_printf(res, "Engine %s could not be found", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		const bool engine_freed_successfully = dyn_containers_free_engine(engine_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!engine_freed_successfully) {
			syslog_server(LOG_ERR, "Request: Remove engine - engine: %s - engine is still in use", 
						  name);

			onion_response_printf(res, "Engine %s is still in use", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		if (!remove_engine_files(name)) {
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine: %s - files could not be removed", name);
						  
			onion_response_printf(res, "Engine %s files could not be removed", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine: %s - engine removed", name);
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine: %s - finished", name);
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
		syslog_server(LOG_ERR, "Interlocker file exists check - Directory %s could not be opened", 
		              interlocker_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, filename) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_NOTICE, "Interlocker file exists check - Interlocker %s already exists",
			              filename);
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
		syslog_server(LOG_NOTICE, "Request: Upload interlocker - interlocker file: %s" , filename);

		if (interlocker_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload interlocker - interlocker file: %s -"
			              " file already exists", filename);
			
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
		syslog_server(LOG_DEBUG, "Request: Upload interlocker - interlocker file: %s - "
		              "copied interlocker BahnDSL file from %s to %s",
		              filename, temp_filepath, final_filepath);

		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".bahn");
		const dynlib_status status = dynlib_compile_bahndsl(filepath, interlocker_dir);
		if (status == DYNLIB_COMPILE_SHARED_BAHNDSL_ERR) {
			syslog_server(LOG_ERR, "Request: Upload interlocker - interlocker file: %s - "
			              "interlocker could not be compiled", filename);
			remove_interlocker_files(libname);
			
			onion_response_printf(res, "Interlocker file %s could not be compiled", filepath);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_DEBUG, "Request: Upload interlocker - interlocker file: %s - "
		              "interlocker compiled", filename);

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_free_interlocker_slot();
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			remove_interlocker_files(libname);

			syslog_server(LOG_ERR, "Request: Upload interlocker - interlocker file: %s - "
			              "no available interlocker slot", filename);
			
			onion_response_printf(res, "No available interlocker slot");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		snprintf(filepath, sizeof(filepath), "%s/%s", interlocker_dir, libname);
		dyn_containers_set_interlocker(interlocker_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		syslog_server(LOG_NOTICE, "Request: Upload interlocker - interlocker file: %s - finished",
		              filename);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Upload interlocker - system not running "
		              "or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}

// What is being refreshed by this? Shouldn't it just be get_interlockers?
onion_connection_status handler_refresh_interlockers(void *_, onion_request *req,
	                                                 onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Refresh interlockers");
		GString *interlockers = dyn_containers_get_interlockers();
		onion_response_printf(res, "%s", interlockers->str);
		g_string_free(interlockers, true);
		syslog_server(LOG_INFO, "Request: Refresh interlockers - finished");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Refresh interlockers - system not running "
		              "or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_remove_interlocker(void *_, onion_request *req,
	                                               onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "interlocker-name");
		if (name == NULL || plugin_is_unremovable(name)) {
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker name is invalid "
			              "or interlocker is unremovable", name);
			
			onion_response_printf(res, "Interlocker name is invalid or interlocker is unremovable");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker: %s", name);

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_interlocker_slot(name);
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker: %s - "
			              "interlocker could not be found", name);
			
			onion_response_printf(res, "Interlocker %s could not be found", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		const bool interlocker_freed_successfully = dyn_containers_free_interlocker(interlocker_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!interlocker_freed_successfully) {
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker: %s - "
			              "interlocker is still in use", name);
			
			onion_response_printf(res, "Interlocker %s is still in use", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}

		if (!remove_interlocker_files(name)) {
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker: %s - "
			              "files could not be removed", name);
			
			onion_response_printf(res, "Interlocker %s files could not be removed", name);
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker: %s - removed",
		              name);
		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker: %s - finished",
		              name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove interlocker - system not running "
		              "or wrong request type");
		
		onion_response_printf(res, "System not running or wrong request type");
		onion_response_set_code(res, HTTP_BAD_REQUEST);
		return OCS_PROCESSED;
	}
}
