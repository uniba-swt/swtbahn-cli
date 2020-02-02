#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include "dynlib.h"
#include "server.h"

const char dynlib_symbol_reset[] = "reset";
const char dynlib_symbol_tick[] = "tick";


// TODO: Compiles C code from a given SCCharts model
dynlib_status dynlib_compile_scchart(dynlib_data *library, const char filepath[]) {
	strncpy(library->filepath, filepath, PATH_MAX + NAME_MAX);
	
	return DYNLIB_COMPILE_SUCCESS;
}

// FIXME: UNTESTED: Compiles a dynamic library from a given C file
dynlib_status dynlib_compile_c(dynlib_data *library, const char filepath[]) {
	strncpy(library->filepath, filepath, PATH_MAX + NAME_MAX);
	
	char command[MAX_INPUT + 2 * (PATH_MAX + NAME_MAX)];
	sprintf(command, "gcc -c -fpic %s.c", library->filepath);
	if (system(command) == -1) {
		return DYNLIB_COMPILE_OBJ_ERR;
	}
	
	sprintf(command, "gcc -shared -fpic -o %s.so %s.c", library->filepath, library->filepath);
	if (system(command) == -1) {
		return DYNLIB_COMPILE_SHARED_ERR;
	}
	
	return DYNLIB_COMPILE_SUCCESS;
}

// Loads a dynamic library from a given filepath
dynlib_status dynlib_load(dynlib_data *library, const char name[], const char filepath[]) {
	// Make sure no library has been loaded
	dynlib_close(library);
	
	// Clear the last error that occured in the dynamic linking loader
	// THREAD SAFETY: This probably needs to be in a semaphore
	dlerror();
	
	// Try and load the dynamic library with *.so extension
	strncpy(library->name, name, NAME_MAX);
	sprintf(library->filepath, "%s.so", filepath);
	library->lib_handle = dlopen(library->filepath, RTLD_LAZY);
	if (library->lib_handle == NULL) {
		// Try and load the dynamic library  with *.dylib extension
		sprintf(library->filepath, "%s.dylib", filepath);
		library->lib_handle = dlopen(library->filepath, RTLD_LAZY);
		
		if (library->lib_handle == NULL) {
			syslog_server(LOG_ERR, "Could not load dynamic library %s.\n%s", library->filepath, dlerror());
			return DYNLIB_LOAD_ERR;
		}
	}
	
	// Try and locate the functions of the library interface
	
	/* Since the value of a symbol in the dynamic library could 
	 * actually be NULL (so that a NULL return from dlsym() need not 
	 * indicate an error), the correct way to test for an error is 
	 * to call dlerror() to clear any old error conditions, then 
	 * call dlsym(), and then call dlerror() again, saving its return 
	 * value into a variable, and check whether this saved value is not NULL.
	 */
	dlerror();
	
	*(void **) (&library->reset_func) = dlsym(library->lib_handle, dynlib_symbol_reset);
	char *error;
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_reset, error);
		return DYNLIB_LOAD_RESET_ERR;
	}
	
	dlerror();
	*(void **) (&library->tick_func) = dlsym(library->lib_handle, dynlib_symbol_tick);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_tick, error);
		return DYNLIB_LOAD_TICK_ERR;
	}
	
	return DYNLIB_LOAD_SUCCESS;
}

bool dynlib_is_loaded(dynlib_data *library) {
	return (library->lib_handle != NULL);
}

void dynlib_close(dynlib_data *library) {
	if (dynlib_is_loaded(library)) {
		dlclose(library->lib_handle);
		library->lib_handle = NULL;
	}
}

void dynlib_reset(dynlib_data *library, TickData *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->reset_func)(tick_data);
	}
}

void dynlib_tick(dynlib_data *library, TickData *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->tick_func)(tick_data);
	}
}

