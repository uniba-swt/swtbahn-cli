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


const static char engine_dir[] = "engines";
const static char engine_extensions[][5] = { "c", "h", "sctx" };
const static int engine_extensions_count = 3;

const static char interlocker_dir[] = "interlockers";
const static char interlocker_extensions[][5] = { "bahn" };
const static int interlocker_extensions_count = 1;


extern pthread_mutex_t dyn_containers_mutex;


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

onion_connection_status handler_upload_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *filename = onion_request_get_post(req, "file");
		const char *temp_filepath = onion_request_get_file(req, "file");
		if (filename == NULL || temp_filepath == NULL) {
			syslog_server(LOG_ERR, "Request: Upload - engine file is invalid");
			return OCS_NOT_IMPLEMENTED;
		}
  
 		if (engine_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload - engine file already exists");
			return OCS_NOT_IMPLEMENTED;
		}  
		
		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", engine_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_NOTICE, "Request: Upload - copied engine SCCharts file from %s to %s", 
					  temp_filepath, final_filepath);

		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".sctx");
		dynlib_status status = dynlib_compile_scchart(filepath, engine_dir);
		if (status == DYNLIB_COMPILE_SCCHARTS_C_ERR || status == DYNLIB_COMPILE_SHARED_SCCHARTS_ERR) {
			syslog_server(LOG_ERR, "Request: Upload - engine file %s could not be compiled", filepath);
			return OCS_NOT_IMPLEMENTED;
		}
		syslog_server(LOG_NOTICE, "Request: Upload - engine %s compiled", filename);
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_free_engine_slot();
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Upload - no available engine slot");
			return OCS_NOT_IMPLEMENTED;
		}
		
		snprintf(final_filepath, sizeof(final_filepath), "%s/lib%s", engine_dir, filename);
		remove_file_extension(filepath, final_filepath, ".sctx");
		dyn_containers_set_engine(engine_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		return OCS_PROCESSED;			
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
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
		if (name == NULL && engine_is_unremovable(name)) {
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine name is invalid or engine is unremovable", 
						  name);
			return OCS_NOT_IMPLEMENTED;
		}
		
		pthread_mutex_lock(&dyn_containers_mutex);
		const int engine_slot = dyn_containers_get_engine_slot(name);
		if (engine_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, 
			              "Request: Remove engine - engine %s could not be found", 
						  name);
			return OCS_NOT_IMPLEMENTED;
		}
		
		const bool engine_freed_successfully = dyn_containers_free_engine(engine_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!engine_freed_successfully) {
			syslog_server(LOG_ERR, "Request: Remove engine - engine %s is still in use", 
						  name);
			return OCS_NOT_IMPLEMENTED;
		}
		
		if (!remove_engine_files(name)) {
			syslog_server(LOG_ERR, "Request: Remove engine - engine %s files could not be removed", 
						  name);
			return OCS_NOT_IMPLEMENTED;
		}
		
		syslog_server(LOG_NOTICE, "Request: Remove engine - engine %s removed", 
					  name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove engine - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
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
	// Remove the prefix "libinterlocking_"
	char name[PATH_MAX + NAME_MAX];
	strcpy(name, library_name + 16);

	int result = 0;
	char filepath[PATH_MAX + NAME_MAX];
	for (int i = 0; i < interlocker_extensions_count; i++) {
		sprintf(filepath, "%s/%s.%s", interlocker_dir, name, interlocker_extensions[i]);
		result = remove(filepath);
	}
	sprintf(filepath, "%s/libinterlocking_%s.so", interlocker_dir, name);
	result += remove(filepath);

	return (result == 0);
}

bool interlocker_is_unremovable(const char name[]) {
	bool result = strcmp(name, "interlocker_deafault (unremovable)") == 0;
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
			return OCS_NOT_IMPLEMENTED;
		}

		if (interlocker_file_exists(filename)) {
			syslog_server(LOG_ERR, "Request: Upload - interlocker file already exists");
			return OCS_NOT_IMPLEMENTED;
		}

		char final_filepath[PATH_MAX + NAME_MAX];
		snprintf(final_filepath, sizeof(final_filepath), "%s/%s", interlocker_dir, filename);
		onion_shortcut_rename(temp_filepath, final_filepath);
		syslog_server(LOG_NOTICE, "Request: Upload - copied interlocker BahnDSL file from %s to %s",
		              temp_filepath, final_filepath);

		char filepath[sizeof(final_filepath)];
		remove_file_extension(filepath, final_filepath, ".bahn");
		dynlib_status status = dynlib_compile_bahndsl(filepath, interlocker_dir);
		if (status == DYNLIB_COMPILE_SHARED_BAHNDSL_ERR) {
			syslog_server(LOG_ERR, "Request: Upload - interlocker file %s could not be compiled", filepath);
			return OCS_NOT_IMPLEMENTED;
		}
		syslog_server(LOG_NOTICE, "Request: Upload - interlocker %s compiled", filename);

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_free_interlocker_slot();
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Upload - no available interlocker slot");
			return OCS_NOT_IMPLEMENTED;
		}

		snprintf(final_filepath, sizeof(final_filepath), "%s/libinterlocking_%s", interlocker_dir, filename);
		remove_file_extension(filepath, final_filepath, ".bahn");
		dyn_containers_set_interlocker(interlocker_slot, filepath);
		pthread_mutex_unlock(&dyn_containers_mutex);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
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
		if (name == NULL && interlocker_is_unremovable(name)) {
			syslog_server(LOG_ERR, "Request: Remove interlocker - interlocker name is invalid or interlocker is unremovable", name);
			return OCS_NOT_IMPLEMENTED;
		}

		pthread_mutex_lock(&dyn_containers_mutex);
		const int interlocker_slot = dyn_containers_get_interlocker_slot(name);
		if (interlocker_slot < 0) {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s could not be found", 
			              name);
			return OCS_NOT_IMPLEMENTED;
		}

		const bool interlocker_freed_successfully = dyn_containers_free_interlocker(interlocker_slot);
		pthread_mutex_unlock(&dyn_containers_mutex);
		if (!interlocker_freed_successfully) {
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s is still in use", 
			              name);
			return OCS_NOT_IMPLEMENTED;
		}

		if (!remove_interlocker_files(name)) {
			syslog_server(LOG_ERR, 
			              "Request: Remove interlocker - interlocker %s files could not be removed", 
			              name);
			return OCS_NOT_IMPLEMENTED;
		}

		syslog_server(LOG_NOTICE, "Request: Remove interlocker - interlocker %s removed",
		              name);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Remove interlocker - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
