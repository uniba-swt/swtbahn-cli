/*
 *
 * Copyright (C) 2021 University of Bamberg, Software Technologies Research Group
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

#ifndef DYNLIB_H
#define DYNLIB_H

#include <stdbool.h>
#include <limits.h>

#include "tick_data.h"

typedef enum {
	DYNLIB_COMPILE_SUCCESS,
	DYNLIB_LOAD_SUCCESS,

	// Could not compile SCCharts model into C file
	DYNLIB_COMPILE_SCCHARTS_C_ERR,
	
	// Could not compile SCCharts' C file into shared library
	DYNLIB_COMPILE_SHARED_SCCHARTS_ERR,
	
	// Could not compile Bahn DSL model into shared library
	DYNLIB_COMPILE_SHARED_BAHNDSL_ERR,
	
	// Could not load the shared library
	DYNLIB_LOAD_ERR,
	
	// Could not find address of reset(...)
	DYNLIB_LOAD_RESET_ERR,
	
	// Could not find address of tick(...)
	DYNLIB_LOAD_TICK_ERR
} dynlib_status;

typedef enum {
	TRAIN_ENGINE,
	INTERLOCKER,
	DRIVE_ROUTE
} dynlib_type;

typedef struct {
	dynlib_type type;

	char name[NAME_MAX];

	// File path of library source code
	char filepath[PATH_MAX + NAME_MAX];

	// Handle to the dynamic library
	void *lib_handle;

	// Library interface functions for a train engine
	void (*train_engine_reset_func)(TickData_train_engine *);
	void (*train_engine_tick_func)(TickData_train_engine *);

	// Library interface functions for an interlocker
	void (*interlocker_reset_func)(TickData_interlocker *);
	void (*interlocker_tick_func)(TickData_interlocker *);

	// Library interface functions for a drive route
	void (*drive_route_reset_func)(TickData_drive_route *);
	void (*drive_route_tick_func)(TickData_drive_route *);
} dynlib_data;


dynlib_status dynlib_compile_scchart(const char filepath[], const char output_dir[]);
dynlib_status dynlib_compile_bahndsl(const char filepath[], const char output_dir[]);

dynlib_status dynlib_load(dynlib_data *library, const char filepath[], dynlib_type type);
bool dynlib_is_loaded(dynlib_data *library);
void dynlib_close(dynlib_data *library);

void dynlib_train_engine_reset(dynlib_data *library, TickData_train_engine *tick_data);
void dynlib_train_engine_tick(dynlib_data *library, TickData_train_engine *tick_data);

void dynlib_interlocker_reset(dynlib_data *library, TickData_interlocker *tick_data);
void dynlib_interlocker_tick(dynlib_data *library, TickData_interlocker *tick_data);

void dynlib_drive_route_reset(dynlib_data *library, TickData_drive_route *tick_data);
void dynlib_drive_route_tick(dynlib_data *library, TickData_drive_route *tick_data);

#endif	// DYNLIB_H
