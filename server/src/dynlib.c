#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <libgen.h>

#include "dynlib.h"
#include "server.h"

const char dynlib_symbol_reset[] = "reset";
const char dynlib_symbol_tick[] = "tick";

const char compiler_output_dir[] = "engines";
const char sccharts_compiler_command[] = "java -jar \"$KICO_PATH\"/kico.jar -s de.cau.cs.kieler.sccharts.netlist";
const char c_compiler_command[] = "clang -shared -fpic -Wall -Wextra";

dynlib_status dynlib_load_train_engine_funcs(dynlib_data *library);


// Compiles a given SCCharts model into a dynamic library
dynlib_status dynlib_compile_scchart_to_c(const char filepath[]) {
	// Get the filename
	char filepath_copy[PATH_MAX + NAME_MAX];
	strncpy(filepath_copy, filepath, PATH_MAX + NAME_MAX);
	const char *filename = basename(filepath_copy);
	
	// Compile the SCCharts model to a C file
	char command[MAX_INPUT + 2 * (PATH_MAX + NAME_MAX)];
	sprintf(command, "%s -o %s %s.sctx", sccharts_compiler_command, compiler_output_dir, filepath);

	int ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_C_ERR;
	}

	// Compile the C file into a dynamic library
	sprintf(command, "%s -o %s/lib%s.so %s/%s.c", 
			c_compiler_command, 
			compiler_output_dir, filename, 
			compiler_output_dir, filename);
	
	ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != 0) {
		return DYNLIB_COMPILE_SHARED_ERR;
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
	}
	
	return status;
}

dynlib_status dynlib_load_train_engine_funcs(dynlib_data *library) {
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
		library->name[0] = '\0';
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

dynlib_status dynlib_load_interlocking(dynlib_data *library, const char filepath[]) {
    char path[PATH_MAX + NAME_MAX];
    sprintf(path, "%s.so", filepath);

    // open on linux
    library->lib_handle = dlopen(path, RTLD_LAZY);
    if (library->lib_handle == NULL) {
        // open on macOS
        sprintf(path, "%s.dylib", filepath);
        library->lib_handle = dlopen(path, RTLD_LAZY);
    }

    if (library->lib_handle == NULL) {
        syslog_server(LOG_ERR, "Could not load dynamic library %s.\n%s", filepath, dlerror());
        return DYNLIB_LOAD_ERR;
    }

    // load symbols
    *(void **) (&library->request_reset_func) = dlsym(library->lib_handle, "request_route_reset");
    *(void **) (&library->request_tick_func) = dlsym(library->lib_handle, "request_route_tick");
    *(void **) (&library->drive_reset_func) = dlsym(library->lib_handle, "drive_route_reset");
    *(void **) (&library->drive_tick_func) = dlsym(library->lib_handle, "drive_route_tick");

    // check
    if (library->request_reset_func == NULL
        || library->request_tick_func == NULL
        || library->drive_reset_func == NULL
        || library->drive_tick_func == NULL) {
        syslog_server(LOG_ERR, "Could not find interlocking symbols.\n%s", dlerror());
        return DYNLIB_LOAD_ERR;
    }

    return DYNLIB_LOAD_SUCCESS;
}

