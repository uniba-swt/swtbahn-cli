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

extern pthread_mutex_t dyn_containers_mutex;


bool engine_file_exists(const char name[]) {
	DIR *dir_handle = opendir(engine_dir);
	if (dir_handle == NULL) {
		closedir(dir_handle);
		syslog_server(LOG_ERR, "Upload: Directory %s could not be opened", engine_dir);
		return true;
	}
	struct dirent *dir_entry = NULL;
	while ((dir_entry = readdir(dir_handle)) != NULL) {
		if (strcmp(dir_entry->d_name, name) == 0) {
			closedir(dir_handle);
			syslog_server(LOG_ERR, "Upload: Engine %s already exists", name);
			return true;
		}
	}
	closedir(dir_handle);
	return false;
}

void remove_file_extension(char filepath_destination[], 
                           const char filepath_source[], const char extension[]) {
    size_t filepath_len = strlen(filepath_source);
    size_t extension_len = strlen(extension);
    
    if (strcmp(filepath_source + filepath_len - extension_len, extension) == 0) {
		strncpy(filepath_destination, filepath_source, filepath_len - extension_len);
    }
}

onion_connection_status handler_upload_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "file");
		const char *filename = onion_request_get_file(req, "file");
		pthread_mutex_lock(&dyn_containers_mutex);
		if (name != NULL && filename != NULL && !engine_file_exists(name)) {
			const int engine_slot = dyn_containers_get_free_engine_slot();
			if (engine_slot >= 0) {
				char final_filename[PATH_MAX + NAME_MAX];
				snprintf(final_filename, sizeof(final_filename), "%s/%s", engine_dir, name);
				onion_shortcut_rename(filename, final_filename);
				syslog_server(LOG_NOTICE, "Request: Upload - copied engine SCCharts file from %s to %s", 
				              filename, final_filename);

				char filepath[sizeof(final_filename)];
				remove_file_extension(filepath, final_filename, ".sctx");
				dynlib_status status = dynlib_compile_scchart_to_c(filepath);
				if (status != DYNLIB_COMPILE_C_ERR && status != DYNLIB_COMPILE_SHARED_ERR) {
					syslog_server(LOG_NOTICE, "Request: Upload - engine %s compiled", name);
					
					snprintf(final_filename, sizeof(final_filename), "%s/lib%s", engine_dir, name);
					remove_file_extension(filepath, final_filename, ".sctx");
					dyn_containers_set_engine(engine_slot, filepath);
					pthread_mutex_unlock(&dyn_containers_mutex);
					return OCS_PROCESSED;
				} else {
					pthread_mutex_unlock(&dyn_containers_mutex);
					syslog_server(LOG_ERR, "Request: Upload - engine file could not be uploaded");
					return OCS_NOT_IMPLEMENTED;
				}
			} else {
				pthread_mutex_unlock(&dyn_containers_mutex);
				syslog_server(LOG_ERR, "Request: Upload - no available engine slot");
				return OCS_NOT_IMPLEMENTED;
			}
		} else {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Upload - engine file is invalid");
			return OCS_NOT_IMPLEMENTED;
		}
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
		syslog_server(LOG_ERR, "Request: Refresh engine - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_remove_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *name = onion_request_get_post(req, "engine-name");
		pthread_mutex_lock(&dyn_containers_mutex);
		if (name != NULL) {
			const int engine_slot = dyn_containers_get_engine_slot(name);
			if (engine_slot >= 0) {
				if (dyn_containers_free_engine(engine_slot)) {
					pthread_mutex_unlock(&dyn_containers_mutex);
					syslog_server(LOG_NOTICE, "Request: Remove engine - engine %s removed", 
				                  name);
					return OCS_PROCESSED;	
				} else {
					pthread_mutex_unlock(&dyn_containers_mutex);
					syslog_server(LOG_ERR, "Request: Remove engine - engine %s is still in use or is unremovable", 
								  name);
					return OCS_NOT_IMPLEMENTED;
				}
			} else {
				pthread_mutex_unlock(&dyn_containers_mutex);
				syslog_server(LOG_ERR, "Request: Remove engine - engine %s could not be found", 
				              name);
				return OCS_NOT_IMPLEMENTED;
			}
		} else {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Remove engine - engine name is invalid", 
						  name);
			return OCS_NOT_IMPLEMENTED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Remove engine - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

