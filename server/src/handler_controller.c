/*
 *
 * Copyright (C) 2017 University of Bamberg, Software Technologies Research Group
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
 * - Nicolas Gross <https://github.com/nicolasgross>
 *
 */

#include <onion/onion.h>
#include <bidib.h>
#include <pthread.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>

#include "server.h"
#include "interlocking.h"

pthread_mutex_t interlocker_mutex = PTHREAD_MUTEX_INITIALIZER;

int grant_route(const char *source_name, const char *destination_name) {
	pthread_mutex_lock(&interlocker_mutex);

	// Get a route from the interlocking table
	int route_id = interlocking_table_get_route_id(source_name, destination_name);
	if (route_id == -1) {
		pthread_mutex_unlock(&interlocker_mutex);
		return -1;
	}
	
	printf("Route ID: %d\n", route_id);

	
	
	// Check whether the route can be granted (KIELER/SCCharts)
	
	interlocking_table_ultraloop[route_id].is_blocked = true;
	
	pthread_mutex_unlock(&interlocker_mutex);

	// Submit the granted route for execution
	
	
	
	
	// Return the ID of the granted route
	return route_id;
}

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_point == NULL || data_state == NULL) {
			syslog(LOG_ERR, "Request: Set point - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_switch_point(data_point, data_state)) {
				syslog(LOG_ERR, "Request: Set point - invalid parameters");
				bidib_flush();
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog(LOG_NOTICE, "Request: Set point - point: %s state: %s",
				       data_point, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog(LOG_ERR, "Request: Set point - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res) {
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_signal = onion_request_get_post(req, "signal");
		const char *data_state = onion_request_get_post(req, "state");
		if (data_signal == NULL || data_state == NULL) {
			syslog(LOG_ERR, "Request: Set signal - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			if (bidib_set_signal(data_signal, data_state)) {
				syslog(LOG_ERR, "Request: Set signal - invalid parameters");
				return OCS_NOT_IMPLEMENTED;
			} else {
				syslog(LOG_NOTICE, "Request: Set signal - signal: %s state: %s",
				       data_signal, data_state);
				bidib_flush();
				return OCS_PROCESSED;
			}
		}
	} else {
		syslog(LOG_ERR, "Request: Set signal - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

