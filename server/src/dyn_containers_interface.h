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

#define TRAIN_ENGINE_COUNT_MAX			4
#define TRAIN_ENGINE_INSTANCE_COUNT_MAX	5
#define INTERLOCKER_COUNT_MAX			4
#define INTERLOCKER_INSTANCE_COUNT_MAX	4


// Input interface with the environment
typedef struct {
	volatile bool terminate;						// Whether to terminate program execution
	int let_period_us;								// Period of the Logical Execution Time (LET) in microseconds
	
	// Train engine information
	struct t_train_engine_io {
		bool input_load;							// Load the train engine specified by filepath
		bool input_unload;							// Unload the train engine
		char input_filepath[PATH_MAX + NAME_MAX];	// File path of library source code, without the file extension
		
		bool output_in_use;							// Whether the container is still in use
		char output_name[NAME_MAX];					// Name of the train engine
	} train_engines_io[TRAIN_ENGINE_COUNT_MAX];
	
	// Train engine instance information
	struct t_train_engine_instance_io {
		bool input_grab;							// Desire to use this instance
		bool input_release;							// Desire to stop using this instance
		int  input_train_engine_type;				// Desired train engine to use
		int  input_requested_speed;					// Input defined by the train engine
		char input_requested_forwards;				// Input defined by the train engine
		
		bool output_in_use;							// Whether this instance is still in use
		int  output_target_speed;					// Output defined by the train engine
		char output_target_forwards;				// Output defined by the train engine

		int  output_target_speed_pre;				// Previous output defined by the train engine
		char output_target_forwards_pre;			// Previous output defined by the train engine
	} train_engine_instances_io[TRAIN_ENGINE_INSTANCE_COUNT_MAX];
	
	// TODO: Interlocker information
	struct t_interlocker_io {
		bool input_load;							// Load the train engine specified by filepath
	} interlockers_io[INTERLOCKER_COUNT_MAX];
	
	// TODO: Interlocker instance information
	struct t_interlocker_instance_io {
		bool input_grab;							// Desire to use this instance

		bool output_in_use;							// Whether this instance is still in use
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


int dyn_containers_start(void);

void dyn_containers_stop(void);

// Obtains a shared memory segment based on a given key
void dyn_containers_shm_create(t_dyn_shm_config * const shm_config, 
                              const int shm_permissions, const key_t shm_key, 
                              t_dyn_containers_interface ** const shm_payload);

// Detaches the shared memory segment from our data space
void dyn_containers_shm_detach(t_dyn_containers_interface ** const shm_payload);

// Deletes the shared memory segment from our data space
void dyn_containers_shm_delete(t_dyn_shm_config * const shm_config);


// Finds the requested train engine, and finds an available train engine instance to use
int dyn_containers_set_train_engine(t_train_data * const grabbed_train, 
                                     const char *train, const char *engine);

void dyn_containers_free_train_engine_instance(const int dyn_containers_engine_instance);

void dyn_containers_set_train_engine_instance_inputs(const int dyn_containers_engine_instance, 
                                                     const int requested_speed, 
                                                     const char requested_forwards);

#endif	// DYN_CONTAINERS_INTERFACE_H
