#ifndef DYNLIB_H
#define DYNLIB_H

#include <stdbool.h>
#include <limits.h>

#include "tick_data.h"

typedef enum {
	DYNLIB_COMPILE_SUCCESS,
	DYNLIB_LOAD_SUCCESS,

	// Could not compile SCCharts model into C file
	DYNLIB_COMPILE_C_ERR,
	
	// Could not compile library into shared library
	DYNLIB_COMPILE_SHARED_ERR,
	
	// Could not load the dynamic library
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


dynlib_status dynlib_compile_scchart_to_c(const char filepath[]);

dynlib_status dynlib_load(dynlib_data *library, const char filepath[], dynlib_type type);
bool dynlib_is_loaded(dynlib_data *library);
void dynlib_close(dynlib_data *library);

void dynlib_train_engine_reset(dynlib_data *library, TickData_train_engine *tick_data);
void dynlib_train_engine_tick(dynlib_data *library, TickData_train_engine *TickData);

void dynlib_interlocker_reset(dynlib_data *library, TickData_interlocker *tick_data);
void dynlib_interlocker_tick(dynlib_data *library, TickData_interlocker *TickData);

void dynlib_drive_route_reset(dynlib_data *library, TickData_drive_route *tick_data);
void dynlib_drive_route_tick(dynlib_data *library, TickData_drive_route *TickData);

#endif	// DYNLIB_H
