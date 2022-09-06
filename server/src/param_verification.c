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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "param_verification.h"

int params_check_session_id(const char *data_session_id) {
	int client_session_id;
	char *end_client_session_id;
	if (data_session_id != NULL) {
	    client_session_id = strtol(data_session_id, &end_client_session_id, 10);
		if (*end_client_session_id == '\0') {
			return client_session_id;
		}
	}
	return -1;
}

int params_check_grab_id(const char *data_grab_id, int max_trains) {
	int grab_id;
	char *end_grab_id;
	if (data_grab_id == NULL ||
	    (grab_id = strtol(data_grab_id, &end_grab_id, 10)) < 0 ||
	    grab_id >= max_trains || *end_grab_id != '\0') {
		return -1;
	}
	return grab_id;
}

int params_check_speed(const char *data_speed) {
	int speed;
	char *end_speed;
	if (data_speed == NULL || (speed = strtol(data_speed, &end_speed, 10)) < -126 ||
	    speed > 126 || *end_speed != '\0') {
		return 999;
	}
	return speed;
}

int params_check_calibrated_speed(const char *data_speed) {
	int speed;
	char *end_speed;
	if (data_speed == NULL || (speed = strtol(data_speed, &end_speed, 10)) < -9 ||
	    speed > 9 || *end_speed != '\0') {
		return 999;
	}
	return speed;
}

int params_check_state(const char *data_state) {
	int state;
	char *end_state;
	if (data_state == NULL || (state = strtol(data_state, &end_state, 10)) < 0 ||
	    state > 1 || *end_state != '\0') {
		return -1;
	}
	return state;
}

const char *params_check_route_id(const char *data_route_id) {
	int route_id;
	char *end_route_id;
	if (data_route_id == NULL || (route_id = strtol(data_route_id, &end_route_id, 10)) < 0 ||
	    *end_route_id != '\0') {
		return "";
	}
	return data_route_id;
}

const char *params_check_mode(const char *data_mode) {
	if (strcmp(data_mode, "automatic") == 0 || strcmp(data_mode, "manual") == 0) {
		return data_mode;
	}
	return "";
}

bool params_check_is_number(const char *string) {
	if (string == NULL || *string == '\0' || isspace(*string)) {
		return 0;
	}
	
	char *p;
	strtod(string, &p);
	
	return (*p == '\0');
}

