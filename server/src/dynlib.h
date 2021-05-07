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

typedef struct {
	char name[NAME_MAX];

	// File path of library source code
	char filepath[PATH_MAX + NAME_MAX];

	// Handle to the dynamic library
	void *lib_handle;

	// Library interface functions
	void (*reset_func)(TickData *);
	void (*tick_func)(TickData *);
} dynlib_data;

typedef struct {
    // Handle to the dynamic library
    void *lib_handle;

    // Library interface functions
    void (*request_reset_func)(request_route_tick_data *);
    void (*request_tick_func)(request_route_tick_data *);
    void (*drive_reset_func)(drive_route_tick_data *);
    void (*drive_tick_func)(drive_route_tick_data *);
} interlocking_dynlib_data;

dynlib_status dynlib_compile_scchart_to_c(const char filepath[]);

dynlib_status dynlib_load(dynlib_data *library, const char filepath[]);
bool dynlib_is_loaded(dynlib_data *library);
void dynlib_close(dynlib_data *library);
void dynlib_reset(dynlib_data *library, TickData *tick_data);
void dynlib_tick(dynlib_data *library, TickData *TickData);

dynlib_status dynlib_load_interlocking(interlocking_dynlib_data *library, const char filepath[]);
void dynlib_close_interlocking(interlocking_dynlib_data *library);

#endif	// DYNLIB_H
