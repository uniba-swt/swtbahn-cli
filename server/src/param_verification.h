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

#ifndef SWTSERVER_HANDLER_ADMIN_H
#define SWTSERVER_HANDLER_ADMIN_H

#include <stdbool.h>


int params_check_session_id(const char *data_session_id);

int params_check_grab_id(const char *data_grab_id, int max_trains);

int params_check_speed(const char *data_speed);

int params_check_calibrated_speed(const char *data_speed);

int params_check_state(const char *data_state);

const char *params_check_route_id(const char *data_route_id);

bool params_check_is_number(const char *string);

bool params_check_is_bool_string(const char *string);

bool params_check_verification_state(const char *string);

#endif	// SWTSERVER_HANDLER_ADMIN_H

