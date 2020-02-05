#ifndef DYNLIB_H
#define DYNLIB_H

#include <stdbool.h>
#include <limits.h>

#include "tick_data.h"

typedef enum {
	DYNLIB_COMPILE_SUCCESS,
	DYNLIB_LOAD_SUCCESS,
	
	// Could not compile library into object file
	DYNLIB_COMPILE_OBJ_ERR,
	
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
    
	// File path of library source code, without the file extension
    char filepath[PATH_MAX + NAME_MAX];

	// Handle to the dynamic library
    void *lib_handle;
    
    // Library interface functions
    void (*reset_func)(TickData *);
    void (*tick_func)(TickData *);
} dynlib_data;


dynlib_status dynlib_compile_scchart(dynlib_data *library, const char filepath[]);
dynlib_status dynlib_compile_c(dynlib_data *library, const char filepath[]);

dynlib_status dynlib_load(dynlib_data *library, const char filepath[]);
bool dynlib_is_loaded(dynlib_data *library);
void dynlib_set_name(dynlib_data *library, const char name[]);
void dynlib_close(dynlib_data *library);
void dynlib_reset(dynlib_data *library, TickData *tick_data);
void dynlib_tick(dynlib_data *library, TickData *TickData);

#endif	// DYNLIB_H
