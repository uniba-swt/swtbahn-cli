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
	void (*train_engine_reset_func)(TickData *);
	void (*train_engine_tick_func)(TickData *);

    // Library interface functions for an interlocker
    void (*request_reset_func)(request_route_tick_data *);
    void (*request_tick_func)(request_route_tick_data *);

    // Library interface functions for a drive route
    void (*drive_reset_func)(drive_route_tick_data *);
    void (*drive_tick_func)(drive_route_tick_data *);
} dynlib_data;


dynlib_status dynlib_compile_scchart_to_c(const char filepath[]);

dynlib_status dynlib_load(dynlib_data *library, const char filepath[], dynlib_type type);
bool dynlib_is_loaded(dynlib_data *library);
void dynlib_close(dynlib_data *library);
void dynlib_reset(dynlib_data *library, TickData *tick_data);
void dynlib_tick(dynlib_data *library, TickData *TickData);

#endif	// DYNLIB_H
