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
#include <bidib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

#include "server.h"
#include "dynlib.h"
#include "dyn_containers_interface.h"
#include "handler_driver.h"
#include "interlocking.h"


const static char engine_dir[] = "engines";

extern pthread_mutex_t dyn_containers_mutex;



void remove_file_extension(char filepath_destination[], const char filepath_source[], const char extension[]) {
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
		if (name != NULL && filename != NULL) {
			const int engine_slot = dyn_containers_find_free_engine_slot();
			if (engine_slot >= 0) {
				char final_filename[PATH_MAX + NAME_MAX];
				snprintf(final_filename, sizeof(final_filename), "%s/%s", engine_dir, name);
				onion_shortcut_rename(filename, final_filename);
				syslog_server(LOG_NOTICE, "Request: Upload - Copied engine SCCharts file from %s to %s", filename, final_filename);

				char filepath[sizeof(final_filename)];
				remove_file_extension(filepath, final_filename, ".sctx");
				dynlib_status status = dynlib_compile_scchart_to_c(filepath);
				if (status != DYNLIB_COMPILE_C_ERR && status != DYNLIB_COMPILE_SHARED_ERR) {
					syslog_server(LOG_NOTICE, "Request: Upload - Engine %s compiled", name);
					
					snprintf(final_filename, sizeof(final_filename), "%s/lib%s", engine_dir, name);
					remove_file_extension(filepath, final_filename, ".sctx");
					dyn_containers_load_engine(engine_slot, filepath);
					pthread_mutex_unlock(&dyn_containers_mutex);

					GString *train_engine_names = dyn_containers_get_train_engines();
					printf("%s\n", train_engine_names->str);
					g_string_free(train_engine_names, true);
					return OCS_PROCESSED;
				} else {
					pthread_mutex_unlock(&dyn_containers_mutex);
					syslog_server(LOG_ERR, "Request: Upload - Engine file could not be uploaded");
					return OCS_NOT_IMPLEMENTED;
				}
			} else {
				pthread_mutex_unlock(&dyn_containers_mutex);
				syslog_server(LOG_ERR, "Request: Upload - No available engine slot");
				return OCS_NOT_IMPLEMENTED;
			}
		} else {
			pthread_mutex_unlock(&dyn_containers_mutex);
			syslog_server(LOG_ERR, "Request: Upload - Engine file is invalid");
			return OCS_NOT_IMPLEMENTED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Upload - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_refresh_engine(void *_, onion_request *req,
                                               onion_response *res) {
	build_response_header(res);
	return OCS_NOT_IMPLEMENTED;
}

onion_connection_status handler_remove_engine(void *_, onion_request *req,
                                              onion_response *res) {

	build_response_header(res);
	return OCS_NOT_IMPLEMENTED;
}

