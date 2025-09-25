/*
 *
 * Copyright (C) 2020 University of Bamberg, Software Technologies Research Group
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

#ifndef DYN_CONTAINERS_INTERFACE_H
#define DYN_CONTAINERS_INTERFACE_H

#include <sys/types.h>
#include <stdbool.h>
#include <limits.h>

#include "handler_driver.h"
#include "handler_controller.h"
#include "dynlib.h"


// Input interface with the environment
typedef struct {
	volatile bool running;							// Whether the containers are ready
	volatile bool terminate;						// Whether to terminate program execution
	int let_period_us;								// Period of the Logical Execution Time (LET) in microseconds
	
	// Train engine information
	struct t_train_engine_io {
		volatile bool input_load;					// Load the train engine specified by filepath
		volatile bool input_unload;					// Unload the train engine
		char input_filepath[PATH_MAX + NAME_MAX];	// File path of library source code, without the file extension
		
		volatile bool output_in_use;				// Whether the container is still in use
		char output_name[NAME_MAX];					// Name of the train engine
	} train_engines_io[TRAIN_ENGINE_COUNT_MAX];
	
	// Train engine instance information
	struct t_train_engine_instance_io {
		bool input_grab;							// Desire to use this instance
		bool input_release;							// Desire to stop using this instance
		int  input_train_engine_type;				// Desired train engine to use
		int  input_requested_speed;					// Input defined by the train engine
		char input_requested_forwards;				// Input defined by the train engine
		
		volatile bool output_in_use;				// Whether this instance is still in use
		int  output_train_engine_type;				// Train engine type in use
		int  output_nominal_speed;					// Output defined by the train engine
		char output_nominal_forwards;				// Output defined by the train engine

		int  output_nominal_speed_pre;				// Previous output defined by the train engine
		char output_nominal_forwards_pre;			// Previous output defined by the train engine
	} train_engine_instances_io[TRAIN_ENGINE_INSTANCE_COUNT_MAX];
	
	// Interlocker information
	struct t_interlocker_io {
		volatile bool input_load;					// Load the interlocker specified by filepath
		volatile bool input_unload;					// Unload the interlocker
		char input_filepath[PATH_MAX + NAME_MAX];	// File path of library source code, without the file extension

		volatile bool output_in_use;				// Whether the container is still in use
		char output_name[NAME_MAX];					// Name of the interlocker algorithm
	} interlockers_io[INTERLOCKER_COUNT_MAX];
	
	// Interlocker instance information
	struct t_interlocker_instance_io {
		volatile bool input_grab;					// Desire to use this instance
		volatile bool input_release;				// Desire to stop using this instance
		int  input_interlocker_type;				// Desired interlocker to use
		volatile bool input_reset;                  // Desire to reset the interlocker
		char input_src_signal_id[NAME_MAX];			// Input defined by interlocker
		char input_dst_signal_id[NAME_MAX];			// Input defined by interlocker
		char input_train_id[NAME_MAX];				// Input defined by interlocker

		volatile bool output_in_use;				// Whether this instance is still in use
		volatile bool output_has_reset;				// Whether this instance has been reset
		int  output_interlocker_type;				// Interlocker type in use
		char output_route_id[NAME_MAX];				// Output defined by interlocker
		volatile bool output_terminated;			// Output defined by interlocker
	} interlocker_instances_io[INTERLOCKER_INSTANCE_COUNT_MAX];
} t_dyn_containers_interface;

// Information needed to create a shared memory segment
typedef struct {
	int size;			// Size of the shared memory segment, rounded up to the nearest page size
	int permissions;	// Options to use when creating the shared memory segment
	key_t key;			// Value associated with the shared memory segment
	int shmid;			// Identifier of the System V shared memory segment
} t_dyn_shm_config;


extern pthread_mutex_t dyn_containers_mutex;

// Main function of dyn_containers.forec
extern void *forec_dyn_containers(void *_);

int dyn_containers_start(void);

void dyn_containers_stop(void);

bool dyn_containers_is_running(void);

// Obtains a shared memory segment based on a given key
void dyn_containers_shm_create(t_dyn_shm_config *shm_config, 
                              int shm_permissions, key_t shm_key, 
                              t_dyn_containers_interface **shm_payload);

// Detaches the shared memory segment from our data space
void dyn_containers_shm_detach(t_dyn_containers_interface **shm_payload);

// Deletes the shared memory segment from our data space
void dyn_containers_shm_delete(t_dyn_shm_config *shm_config);


// Finds the first available slot for a train engine
// Can only be called while the dyn_containers_mutex is locked
int dyn_containers_get_free_engine_slot(void);

// Finds the slot of a train engine
// Can only be called while the dyn_containers_mutex is locked
int dyn_containers_get_engine_slot(const char name[]);

// Loads train engine into specified slot
// Can only be called while the dyn_containers_mutex is locked
void dyn_containers_set_engine(int engine_slot, const char filepath[]);

// Unloads train engine at specified slot
// Can only be called while the dyn_containers_mutex is locked
bool dyn_containers_free_engine(int engine_slot);

// Gets a char*-GArray with the names of train engines that have been loaded.
GArray *dyn_containers_get_train_engines_arr(void);

// Finds the requested train engine, and finds an available train engine instance to use
int dyn_containers_set_train_engine_instance(t_train_data *grabbed_train, 
                                             const char *train, const char *engine);

void dyn_containers_free_train_engine_instance(int dyn_containers_engine_instance);

void dyn_containers_set_train_engine_instance_inputs(int dyn_containers_engine_instance, 
                                                     int requested_speed, 
                                                     bool requested_forwards);


// Finds the first available slot for a interlocker
// Can only be called while the dyn_containers_mutex is locked
int dyn_containers_get_free_interlocker_slot(void);

// Finds the slot of a interlocker
// Can only be called while the dyn_containers_mutex is locked
int dyn_containers_get_interlocker_slot(const char name[]);

// Loads interlocker into specified slot
// Can only be called while the dyn_containers_mutex is locked
void dyn_containers_set_interlocker(int interlocker_slot, const char filepath[]);

// Unloads interlocker at specified slot
// Can only be called while the dyn_containers_mutex is locked
bool dyn_containers_free_interlocker(int interlocker_slot);

// Gets a char*-GArray with the names of interlockers that have been loaded
GArray *dyn_containers_get_interlockers_arr(void);

// Gets a comma-separated string of interlockers that have been loaded
GString *dyn_containers_get_interlockers(void);

// Finds the requested interlocker, and finds an available interlocker instance to use; 
// returns 1 on error/failure to set interlocker instance, and 0 on success.
int dyn_containers_set_interlocker_instance(t_interlocker_data *interlocker_instance,
                                            const char *interlocker);

void dyn_containers_free_interlocker_instance(t_interlocker_data *interlocker_instance);

void dyn_containers_set_interlocker_instance_reset(t_interlocker_data *interlocker_instance,
                                                   bool reset);

void dyn_containers_set_interlocker_instance_inputs(t_interlocker_data *interlocker_instance, 
                                                    const char *src_signal_id, 
                                                    const char *dst_signal_id,
                                                    const char *train_id);

void dyn_containers_get_interlocker_instance_outputs(t_interlocker_data *interlocker_instance, 
                                                     struct t_interlocker_instance_io *interlocker_instance_io_copy);

#endif  // DYN_CONTAINERS_INTERFACE_H
