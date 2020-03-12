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
#include "handler_driver.h"
#include "interlocking.h"


const static char engine_dir[] = "engines";

static pthread_mutex_t upload_mutex = PTHREAD_MUTEX_INITIALIZER;


onion_connection_status handler_upload_engine(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_POST) {
		const char *name = onion_request_get_post(req, "file");
		const char *filename = onion_request_get_file(req, "file");
		int retval;
		pthread_mutex_lock(&upload_mutex);
		if (name != NULL && filename != NULL) {
			char final_filename[PATH_MAX + NAME_MAX];
			snprintf(final_filename, sizeof(final_filename), "%s/%s", engine_dir, name);
			syslog_server(LOG_NOTICE, "Request: Upload - Copying engine from %s to %s", filename, final_filename);
			onion_shortcut_rename(filename, final_filename);
			syslog_server(LOG_NOTICE, "Request: Upload - Engine %s uploaded", name);
			retval = OCS_PROCESSED;
		} else {
			syslog_server(LOG_ERR, "Request: Upload - Engine could not be uploaded");
			retval = OCS_NOT_IMPLEMENTED;
		}
		pthread_mutex_unlock(&upload_mutex);
		return retval;
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

