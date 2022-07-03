#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <libgen.h>

#include "dynlib.h"
#include "server.h"

static const char dynlib_symbol_train_engine_reset[] = "reset";
static const char dynlib_symbol_train_engine_tick[] = "tick";

static const char dynlib_symbol_interlocker_reset[] = "request_route_reset";
static const char dynlib_symbol_interlocker_tick[] = "request_route_tick";

static const char dynlib_symbol_drive_route_reset[] = "drive_route_reset";
static const char dynlib_symbol_drive_route_tick[] = "drive_route_tick";

static const char sccharts_compiler_c_command[] = "java -jar \"$KIELER_PATH\"/kico.jar -s de.cau.cs.kieler.sccharts.priority";
static const char c_compiler_command[] = "gcc -shared -fpic -Wall -Wextra";
//static const char c_compiler_command[] = "gcc -shared -fpic -Wall -Wextra";
//static const char c_compiler_command[] = "clang-14 -shared -fpic -Wall -Wextra"; //OG 'clang'

static const char bahndsl_compiler_command[] = "bahnc -o %s/bahnc -m library %s/%s.bahn";
static const char bahndsl_move_command[] = "mv %s/bahnc/libinterlocker_%s.so %s/libinterlocker_%s.so";

dynlib_status dynlib_load_train_engine_funcs(dynlib_data *library);
dynlib_status dynlib_load_interlocker_funcs(dynlib_data *library);
dynlib_status dynlib_load_drive_route_funcs(dynlib_data *library);


// Compiles a given SCCharts model into a shared library
dynlib_status dynlib_compile_scchart(const char filepath[], const char output_dir[]) {
	// Get the filename
	char filepath_copy[PATH_MAX + NAME_MAX];
	strncpy(filepath_copy, filepath, PATH_MAX + NAME_MAX);
	const char *filename = basename(filepath_copy);
	
	// Compile the SCCharts model to a C file
	char command[MAX_INPUT + 2 * (PATH_MAX + NAME_MAX)];
	sprintf(command, "%s -o %s %s.sctx", sccharts_compiler_c_command, output_dir, filepath);

	int ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_SCCHARTS_C_ERR;
	}

	// Compile the C file into a shared library
	sprintf(command, "%s -o %s/lib%s.so %s/%s.c", 
			c_compiler_command, 
			output_dir, filename, 
			output_dir, filename);
	
	ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_SHARED_SCCHARTS_ERR;
	}
	
	return DYNLIB_COMPILE_SUCCESS;
}

// Compiles a given BahnDSL model into a shared library
dynlib_status dynlib_compile_bahndsl(const char filepath[], const char output_dir[]) {
	// Get the filename
	char filepath_copy[PATH_MAX + NAME_MAX];
	strncpy(filepath_copy, filepath, PATH_MAX + NAME_MAX);
	const char *filename = basename(filepath_copy);
	
	// Compile the BahnDSL model to a shared library
	char command[MAX_INPUT + 2 * (PATH_MAX + NAME_MAX)];
	sprintf(command, bahndsl_compiler_command, output_dir, output_dir, filename);	
	int ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_SHARED_BAHNDSL_ERR;
	}
	
	sprintf(command, bahndsl_move_command, output_dir, filename, output_dir, filename);
	ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_SHARED_BAHNDSL_ERR;
	}
	
	return DYNLIB_COMPILE_SUCCESS;
}

// Loads a dynamic library from a given filepath
dynlib_status dynlib_load(dynlib_data *library, const char filepath[], dynlib_type type) {
	// Make sure no library has been loaded
	dynlib_close(library);
	
	// Clear the last error that occured in the dynamic linking loader
	// THREAD SAFETY: This probably needs to be in a semaphore
	dlerror();
	
	// Try and load the dynamic library with *.so extension
	sprintf(library->filepath, "%s.so", filepath);
	library->lib_handle = dlopen(library->filepath, RTLD_LAZY);
	if (library->lib_handle == NULL) {
		// Try and load the dynamic library with *.dylib extension
		sprintf(library->filepath, "%s.dylib", filepath);
		library->lib_handle = dlopen(library->filepath, RTLD_LAZY);
		
		if (library->lib_handle == NULL) {
			syslog_server(LOG_ERR, "Could not load dynamic library %s.\n%s", library->filepath, dlerror());
			return DYNLIB_LOAD_ERR;
		}
	}

	library->type = type;
	
	// Try and locate the functions of the library interface
	
	/* Since the value of a symbol in the dynamic library could 
	 * actually be NULL (so that a NULL return from dlsym() need not 
	 * indicate an error), the correct way to test for an error is 
	 * to call dlerror() to clear any old error conditions, then 
	 * call dlsym(), and then call dlerror() again, saving its return 
	 * value into a variable, and check whether this saved value is not NULL.
	 */
	dlerror();
	
	dynlib_status status = DYNLIB_LOAD_ERR;
	if (library->type == TRAIN_ENGINE) {
		status = dynlib_load_train_engine_funcs(library);
	} else if (library->type == INTERLOCKER) {
		status = dynlib_load_interlocker_funcs(library);
	} else if (library->type == DRIVE_ROUTE) {
		status = dynlib_load_drive_route_funcs(library);
	}
	
	if (status == DYNLIB_LOAD_SUCCESS) {
		syslog_server(LOG_NOTICE, "Loaded dynamic library %s\n", library->filepath);
	}
	
	return status;
}

dynlib_status dynlib_load_train_engine_funcs(dynlib_data *library) {
	char *error;

	*(void **) (&library->train_engine_reset_func) = dlsym(library->lib_handle, dynlib_symbol_train_engine_reset);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_train_engine_reset, error);
		return DYNLIB_LOAD_RESET_ERR;
	}
	
	dlerror();
	*(void **) (&library->train_engine_tick_func) = dlsym(library->lib_handle, dynlib_symbol_train_engine_tick);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_train_engine_tick, error);
		return DYNLIB_LOAD_TICK_ERR;
	}

	return DYNLIB_LOAD_SUCCESS;
}

dynlib_status dynlib_load_interlocker_funcs(dynlib_data *library) {
	char *error;

    *(void **) (&library->interlocker_reset_func) = dlsym(library->lib_handle, dynlib_symbol_interlocker_reset);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_interlocker_reset, error);
		return DYNLIB_LOAD_RESET_ERR;
	}

	dlerror();
    *(void **) (&library->interlocker_tick_func) = dlsym(library->lib_handle, dynlib_symbol_interlocker_tick);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_interlocker_tick, error);
		return DYNLIB_LOAD_TICK_ERR;
	}

	return DYNLIB_LOAD_SUCCESS;
}

dynlib_status dynlib_load_drive_route_funcs(dynlib_data *library) {
	char *error;

    *(void **) (&library->drive_route_reset_func) = dlsym(library->lib_handle, dynlib_symbol_drive_route_reset);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_drive_route_reset, error);
		return DYNLIB_LOAD_RESET_ERR;
	}

    *(void **) (&library->drive_route_tick_func) = dlsym(library->lib_handle, dynlib_symbol_drive_route_tick);
	if ((error = dlerror()) != NULL) {
		syslog_server(LOG_ERR, "Could not find address of symbol %s.\n%s", dynlib_symbol_drive_route_tick, error);
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
		library->name[0] = '\0';
	}
}

void dynlib_train_engine_reset(dynlib_data *library, TickData_train_engine *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->train_engine_reset_func)(tick_data);
	}
}

void dynlib_train_engine_tick(dynlib_data *library, TickData_train_engine *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->train_engine_tick_func)(tick_data);
	}
}

void dynlib_interlocker_reset(dynlib_data *library, TickData_interlocker *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->interlocker_reset_func)(tick_data);
	}
}

void dynlib_interlocker_tick(dynlib_data *library, TickData_interlocker *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->interlocker_tick_func)(tick_data);
	}
}

void dynlib_drive_route_reset(dynlib_data *library, TickData_drive_route *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->drive_route_reset_func)(tick_data);
	}
}

void dynlib_drive_route_tick(dynlib_data *library, TickData_drive_route *tick_data) {
	if (dynlib_is_loaded(library)) {
		(*library->drive_route_tick_func)(tick_data);
	}
}
