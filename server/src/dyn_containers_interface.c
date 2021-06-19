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

#include <sys/shm.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <bidib.h>

#include "dyn_containers_interface.h"
#include "handler_admin.h"
#include "server.h"
#include "handler_driver.h"


extern void *forec_dyn_containers(void *_);

pthread_mutex_t dyn_containers_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t dyn_containers_thread;
static pthread_t dyn_containers_actuate_thread;

static t_dyn_containers_interface *dyn_containers_interface;
#define MICROSECOND 1
static const int let_period_us = 100000 * MICROSECOND;	// 0.1 seconds

static t_dyn_shm_config shm_config;
static const int shm_permissions = (IPC_CREAT | 0666);
static const key_t shm_key = 1234; 


void dyn_containers_reset_interface(
        t_dyn_containers_interface * const dyn_containers_interface) {
	dyn_containers_interface->terminate = false;

	dyn_containers_interface->let_period_us = let_period_us;
	
	for (size_t i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		dyn_containers_interface->train_engines_io[i] = 
		(struct t_train_engine_io) {
			.input_load = false, 
			.input_unload = false, 
			
			.output_in_use = false
		};
	}
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		dyn_containers_interface->train_engine_instances_io[i] = 
		(struct t_train_engine_instance_io) {
			.input_grab = false,
			.input_release = false,
			.input_train_engine_type = -1,
			.input_requested_speed = 0,
			.input_requested_forwards = true,
			
			.output_in_use = false,
			.output_nominal_speed = 0,
			.output_nominal_forwards = 0,
			
			.output_nominal_speed_pre = 0,
			.output_nominal_forwards_pre = 0
		};
	}
	
	for (size_t i = 0; i < INTERLOCKER_COUNT_MAX; i++) {
		dyn_containers_interface->interlockers_io[i] = 
		(struct t_interlocker_io) {
			.input_load = false,
			.input_unload = false, 
			
			.output_in_use = false
		};
	}
	for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		dyn_containers_interface->interlocker_instances_io[i] = 
		(struct t_interlocker_instance_io) {
			.input_grab = false,
			.input_release = false,
			.input_interlocker_type = -1,
			.input_reset = false,
			
			.input_src_signal_id = "",
			.input_dst_signal_id = "",
			.input_train_id = "",

			.output_in_use = false,
			.output_has_reset = false,
			.output_route_id = "",
			.output_terminated = false
		};
	}
}

// Execute the outputs of the train engines and interlockers via the BiDiB library
static void *dyn_containers_actuate(void *_) {
	do {
		pthread_mutex_lock(&grabbed_trains_mutex);
		pthread_mutex_lock(&dyn_containers_mutex);

		for (size_t i = 0; i < MAX_TRAINS; i++) {
			if (grabbed_trains[i].is_valid && grabbed_trains[i].name != NULL) {
				const int dyn_containers_engine_instance = 
				   grabbed_trains[i].dyn_containers_engine_instance;
				        
				struct t_train_engine_instance_io * const engine_instance = 
				    &dyn_containers_interface->train_engine_instances_io[dyn_containers_engine_instance];
				if (engine_instance->output_in_use) {
					if (engine_instance->output_nominal_speed != engine_instance->output_nominal_speed_pre
							|| engine_instance->output_nominal_forwards != engine_instance->output_nominal_forwards_pre) {
						if (bidib_set_train_speed(grabbed_trains[i].name->str, 
												  engine_instance->output_nominal_forwards
												  ? engine_instance->output_nominal_speed 
												  : -engine_instance->output_nominal_speed, 
												  grabbed_trains[i].track_output)) {
							syslog_server(LOG_ERR, "Request: Set train speed - bad parameter values");
						} else {
							bidib_flush();
							syslog_server(LOG_NOTICE, "Request: Set train speed - train: %s speed: %d",
								   grabbed_trains[i].name->str, 
								   engine_instance->output_nominal_forwards 
								   ? engine_instance->output_nominal_speed 
								   : -engine_instance->output_nominal_speed);
							engine_instance->output_nominal_speed_pre = engine_instance->output_nominal_speed;
							engine_instance->output_nominal_forwards_pre = engine_instance->output_nominal_forwards;
						}
					}
				}
			}
		}
		
		pthread_mutex_unlock(&dyn_containers_mutex);
		pthread_mutex_unlock(&grabbed_trains_mutex);


		pthread_mutex_lock(&dyn_containers_mutex);
		
		// TODO: Interlockers cannot be run continuously because they will keep granting the same routes according to their inputs.
		for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
			struct t_interlocker_instance_io * const interlocker_instance = 
				&dyn_containers_interface->interlocker_instances_io[i];
			if (interlocker_instance->output_in_use) {
				if (interlocker_instance->output_terminated
						&& !strncmp(interlocker_instance->output_route_id, "", NAME_MAX)) {
					// Interlocker has sent point and signal commands via the BiDiB library, 
					// but we have to flush them to the BiDiB boards
					bidib_flush();
					syslog_server(LOG_NOTICE, "Request: Set points and signals for route id %s - interlocker type %s",
					              interlocker_instance->output_route_id,
								  interlocker_instance->output_interlocker_type);
					// TODO: Clear inputs? Set signals to ""?
				}
			}
		}
			
		pthread_mutex_unlock(&dyn_containers_mutex);

		usleep(let_period_us);
	} while (running);
	
	dyn_containers_interface->terminate = true;
	
	// TODO: Ensure that all trains really stop
	
	pthread_exit(NULL);
}

int dyn_containers_start(void) {
	dyn_containers_shm_create(&shm_config, shm_permissions, shm_key, 
	                         &dyn_containers_interface);
	dyn_containers_reset_interface(dyn_containers_interface);
	pthread_create(&dyn_containers_thread, NULL, 
	               forec_dyn_containers, NULL);
	pthread_create(&dyn_containers_actuate_thread, NULL, 
	               dyn_containers_actuate, NULL);
	return 0;
}

void dyn_containers_stop(void) {
	pthread_join(dyn_containers_actuate_thread, NULL);
	pthread_join(dyn_containers_thread, NULL);
	
	dyn_containers_shm_detach(&dyn_containers_interface);
	dyn_containers_shm_delete(&shm_config);
	syslog_server(LOG_NOTICE, "Closed dynamic library containers");
}

// General function to obtain a shared memory segment based on a given key
void dyn_containers_shm_create(t_dyn_shm_config * const shm_config, 
        const int shm_permissions, const key_t shm_key, 
        t_dyn_containers_interface ** const shm_payload) {
	// Create our shared memory segment with the given shmKey.
	shm_config->size = 1 * sizeof(t_dyn_containers_interface);
	shm_config->permissions = shm_permissions;
	shm_config->key = shm_key;
	shm_config->shmid = shmget(shm_config->key, shm_config->size, 
	                           shm_config->permissions);
	
	if (shm_config->shmid == -1) {
		int error_number = errno;
		syslog_server(LOG_ERR, 
		              "Error getting shared memory segment: errono %d", 
		              error_number);
		return;
	}

	// Attach the shared memory segment to our data space
	*shm_payload = shmat(shm_config->shmid, NULL, 0);
	if (shm_payload == (void *) -1) {
		syslog_server(LOG_ERR, 
		              "Error attaching shared memory segment to process' data space");
		return;
	}	
}

// Detaches the shared memory segment from our data space
void dyn_containers_shm_detach(t_dyn_containers_interface ** const shm_payload) {
	if (shmdt(*shm_payload) == -1) {
		int error_number = errno;
		syslog_server(LOG_ERR, 
		              "Error detaching shared memory segment: errono %d", 
		              error_number);
		return;
	}
	
	*shm_payload = NULL;
}

// Deletes the shared memory segment from our data space
void dyn_containers_shm_delete(t_dyn_shm_config * const shm_config) {
	shm_config->shmid = shmctl(shm_config->shmid, IPC_RMID, NULL);
	if (shm_config->shmid == -1) {
		int error_number = errno;
		syslog_server(LOG_ERR, 
		              "Error deleting shared memory segment: errono %d", 
		              error_number);
		return;
	}
}

// Finds the first available slot for a train engine
// Can only be called while the dyn_containers_mutex is locked
const int dyn_containers_get_free_engine_slot(void) {
	for (int i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		struct t_train_engine_io * const train_engine_io = 
		    &dyn_containers_interface->train_engines_io[i];
		if (!train_engine_io->output_in_use) {
			return i;
		}
	}
	return -1;
}

// Finds the slot of a train engine
// Can only be called while the dyn_containers_mutex is locked
const int dyn_containers_get_engine_slot(const char name[]) {
	for (int i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		struct t_train_engine_io * const train_engine_io = 
		    &dyn_containers_interface->train_engines_io[i];
		if (train_engine_io->output_in_use && 
		    strcmp(train_engine_io->output_name, name) == 0) {
			return i;
		}
	}
	return -1;
}


// Loads train engine into specified slot
// Can only be called while the dyn_containers_mutex is locked
void dyn_containers_set_engine(const int engine_slot, const char filepath[]) {
	struct t_train_engine_io * const train_engine_io = 
	    &dyn_containers_interface->train_engines_io[engine_slot];
	train_engine_io->input_load = true;
	train_engine_io->input_unload = false;
	strcpy(train_engine_io->input_filepath, filepath);
	pthread_mutex_unlock(&dyn_containers_mutex);
	syslog_server(LOG_NOTICE, 
				  "Waiting for train engine %s to be dynamically loaded into slot %d", 
				  filepath, engine_slot);
	while (!train_engine_io->output_in_use) {
		usleep(let_period_us);
	}
	pthread_mutex_lock(&dyn_containers_mutex);
	train_engine_io->input_load = false;
	syslog_server(LOG_NOTICE, 
				  "Train engine %s has been dynamically loaded into engine slot %d", 
				  filepath, engine_slot);
}

// Unloads train engine at specified slot
// Can only be called while the dyn_containers_mutex is locked
bool dyn_containers_free_engine(const int engine_slot) {
	// Check that no instance of the train engine is in use
	for (size_t i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		struct t_train_engine_instance_io * const engine_instance = 
			&dyn_containers_interface->train_engine_instances_io[i];
		if (engine_instance->output_in_use && 
		    engine_instance->output_train_engine_type == engine_slot) {
			return false;
		}
	}

	// Unload the train engine
	struct t_train_engine_io * const train_engine_io = 
	    &dyn_containers_interface->train_engines_io[engine_slot];
	train_engine_io->input_load = false;
	train_engine_io->input_unload = true;
	strcpy(train_engine_io->input_filepath, "");
	pthread_mutex_unlock(&dyn_containers_mutex);
	syslog_server(LOG_NOTICE, 
				  "Waiting for train engine %s at slot %d to be unloaded", 
				  train_engine_io->input_filepath, engine_slot);
	while (train_engine_io->output_in_use) {
		usleep(let_period_us);
	}
	pthread_mutex_lock(&dyn_containers_mutex);
	train_engine_io->input_unload = false;
	syslog_server(LOG_NOTICE, 
				  "Unloaded train engine at slot %d", 
				  engine_slot);
	return true;
}

GString *dyn_containers_get_train_engines(void) {
	GString *train_engine_names = g_string_new(NULL);
	int i = 0;
	for (i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		struct t_train_engine_io * const train_engine_io = 
		    &dyn_containers_interface->train_engines_io[i];
		if (!train_engine_io->output_in_use) {
			continue;
		}
		// Copy string
		if (i != 0) {
			g_string_append(train_engine_names, ",");
		}
		g_string_append(train_engine_names, train_engine_io->output_name);
	}
	return train_engine_names;
}

int dyn_containers_set_train_engine_instance(t_train_data * const grabbed_train, 
                                     const char *train, const char *engine) {
	if (engine == NULL) {
		syslog_server(LOG_ERR, "Could not set train engine because engine was NULL");
		return 1;
	}
	
	pthread_mutex_lock(&dyn_containers_mutex);
	int train_engine_type = -1;
	for (int i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		struct t_train_engine_io * const train_engine_io = 
		    &dyn_containers_interface->train_engines_io[i];
		if (strcmp(train_engine_io->output_name, engine) == 0) {
			train_engine_type = i;
			break;
		}
	}

	if (train_engine_type == -1) {
		pthread_mutex_unlock(&dyn_containers_mutex);
		syslog_server(LOG_ERR, "Engine %s could not be found", engine);
		return 1;
	}
	
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		struct t_train_engine_instance_io * const train_engine_instance_io = 
		    &dyn_containers_interface->train_engine_instances_io[i];
		int instance_in_use = train_engine_instance_io->output_in_use; 
		if (!instance_in_use) {
			train_engine_instance_io->input_grab = true;
			train_engine_instance_io->input_train_engine_type = train_engine_type;
			train_engine_instance_io->input_requested_speed = 0;
			train_engine_instance_io->input_requested_forwards = true;
			pthread_mutex_unlock(&dyn_containers_mutex);

			do {
				usleep(let_period_us);
				instance_in_use = train_engine_instance_io->output_in_use;
			} while (!instance_in_use);
			
			pthread_mutex_lock(&dyn_containers_mutex);
			train_engine_instance_io->input_grab = false;
			pthread_mutex_unlock(&dyn_containers_mutex);
			
			grabbed_train->dyn_containers_engine_instance = i;
			syslog_server(LOG_NOTICE, "Train %s has engine %s", train, engine);
			return 0;
		}
	}

	pthread_mutex_unlock(&dyn_containers_mutex);
	syslog_server(LOG_ERR, "No engine instances available for train %s", train);
	return 1;
}

void dyn_containers_free_train_engine_instance(const int dyn_containers_engine_instance) {
	if (dyn_containers_interface == NULL) {
		return;
	}
	
	struct t_train_engine_instance_io * const train_engine_instance_io = 
		&dyn_containers_interface->train_engine_instances_io[dyn_containers_engine_instance];

	pthread_mutex_lock(&dyn_containers_mutex);
	train_engine_instance_io->input_release = true;
	pthread_mutex_unlock(&dyn_containers_mutex);

	int instance_in_use = true;
	do {
		usleep(let_period_us);
		instance_in_use = train_engine_instance_io->output_in_use;
	} while (instance_in_use);
	
	pthread_mutex_lock(&dyn_containers_mutex);
	train_engine_instance_io->input_release = false;
	pthread_mutex_unlock(&dyn_containers_mutex);
}

void dyn_containers_set_train_engine_instance_inputs(const int dyn_containers_engine_instance, 
                                                     const int requested_speed, 
                                                     const char requested_forwards) {
	struct t_train_engine_instance_io * const train_engine_instance_io = 
		&dyn_containers_interface->train_engine_instances_io[dyn_containers_engine_instance];

	pthread_mutex_lock(&dyn_containers_mutex);
	train_engine_instance_io->input_requested_speed = requested_speed;
	train_engine_instance_io->input_requested_forwards = requested_forwards;
	pthread_mutex_unlock(&dyn_containers_mutex);
}

// Finds the first available slot for a interlocker
// Can only be called while the dyn_containers_mutex is locked
const int dyn_containers_get_free_interlocker_slot(void) {
	for (int i = 0; i < INTERLOCKER_COUNT_MAX; i++) {
		struct t_interlocker_io * const interlocker_io =
			&dyn_containers_interface->interlockers_io[i];
		if (!interlocker_io->output_in_use) {
			return i;
		}
	}
	return -1;
}

// Finds the slot of a interlocker
// Can only be called while the dyn_containers_mutex is locked
const int dyn_containers_get_interlocker_slot(const char name[]) {
	for (int i = 0; i < INTERLOCKER_COUNT_MAX; i++) {
		struct t_interlocker_io * const interlocker_io =
			&dyn_containers_interface->interlockers_io[i];
		if (interlocker_io->output_in_use &&
			strcmp(interlocker_io->output_name, name) == 0) {
			return i;
		}
	}
	return -1;
}

// Loads interlocker into specified slot
// Can only be called while the dyn_containers_mutex is locked
void dyn_containers_set_interlocker(const int interlocker_slot, const char filepath[]) {
	struct t_interlocker_io * const interlocker_io =
		&dyn_containers_interface->interlockers_io[interlocker_slot];
	interlocker_io->input_load = true;
	interlocker_io->input_unload = false;
	strcpy(interlocker_io->input_filepath, filepath);
	pthread_mutex_unlock(&dyn_containers_mutex);
	syslog_server(LOG_NOTICE,
	              "Waiting for interlocker %s to be dynamically loaded into slot %d",
	              filepath, interlocker_slot);
	while (!interlocker_io->output_in_use) {
		usleep(let_period_us);
	}
	pthread_mutex_lock(&dyn_containers_mutex);
	interlocker_io->input_load = false;
	syslog_server(LOG_NOTICE,
	              "Interlocker %s has been dynamically loaded into interlocker slot %d",
	              filepath, interlocker_slot);
}

// Unloads interlocker at specified slot
// Can only be called while the dyn_containers_mutex is locked
bool dyn_containers_free_interlocker(const int interlocker_slot) {
	// Check that no instance of the interlocker is in use
	for (size_t i = 0; i < INTERLOCKER_INSTANCE_COUNT_MAX; i++) {
		struct t_interlocker_instance_io * const interlocker_instance =
			&dyn_containers_interface->interlocker_instances_io[i];
		if (interlocker_instance->output_in_use &&
			interlocker_instance->output_interlocker_type == interlocker_slot) {
			return false;
		}
	}

	// Unload the interlocker
	struct t_interlocker_io * const interlocker_io = 
	    &dyn_containers_interface->interlockers_io[interlocker_slot];
	interlocker_io->input_load = false;
	interlocker_io->input_unload = true;
	strcpy(interlocker_io->input_filepath, "");
	pthread_mutex_unlock(&dyn_containers_mutex);
	syslog_server(LOG_NOTICE, 
				  "Waiting for interlocker %s at slot %d to be unloaded", 
				  interlocker_io->input_filepath, interlocker_slot);
	while (interlocker_io->output_in_use) {
		usleep(let_period_us);
	}
	pthread_mutex_lock(&dyn_containers_mutex);
	interlocker_io->input_unload = false;
	syslog_server(LOG_NOTICE, 
				  "Unloaded interlocker at slot %d", 
				  interlocker_slot);
	return true;
}

GString *dyn_containers_get_interlockers(void) {
	GString *interlocker_names = g_string_new(NULL);
	int i = 0;
	for (i = 0; i < INTERLOCKER_COUNT_MAX; i++) {
		struct t_interlocker_io * const interlocker_io =
			&dyn_containers_interface->interlockers_io[i];
		if (!interlocker_io->output_in_use) {
			continue;
		}
		// Copy string
		if (i != 0) {
			g_string_append(interlocker_names, ",");
		}
		g_string_append(interlocker_names, interlocker_io->output_name);
	}
	return interlocker_names;
}

void dyn_containers_free_interlocker_instance(const int dyn_containers_interlocker_instance) {
	if (dyn_containers_interface == NULL) {
		return;
	}
	
	struct t_interlocker_instance_io * const interlocker_instance_io = 
		&dyn_containers_interface->interlocker_instances_io[dyn_containers_interlocker_instance];

	pthread_mutex_lock(&dyn_containers_mutex);
	interlocker_instance_io->input_release = true;
	pthread_mutex_unlock(&dyn_containers_mutex);

	int instance_in_use = true;
	do {
		usleep(let_period_us);
		instance_in_use = interlocker_instance_io->output_in_use;
	} while (instance_in_use);
	
	pthread_mutex_lock(&dyn_containers_mutex);
	interlocker_instance_io->input_release = false;
	pthread_mutex_unlock(&dyn_containers_mutex);
}

void dyn_containers_set_interlocker_instance_inputs(const int dyn_containers_interlocker_instance, 
                                                    const char *src_signal_id, 
                                                    const char *dst_signal_id,
                                                    const char *train_id) {
	struct t_interlocker_instance_io * const interlocker_instance_io = 
		&dyn_containers_interface->interlocker_instances_io[dyn_containers_interlocker_instance];

	pthread_mutex_lock(&dyn_containers_mutex);
	strncpy(interlocker_instance_io->input_src_signal_id, src_signal_id, NAME_MAX);
	strncpy(interlocker_instance_io->input_dst_signal_id, dst_signal_id, NAME_MAX);
	strncpy(interlocker_instance_io->input_train_id, train_id, NAME_MAX);
	pthread_mutex_unlock(&dyn_containers_mutex);
}

void dyn_containers_get_interlocker_instance_outputs(const int dyn_containers_interlocker_instance, 
                                                     struct t_interlocker_instance_io interlocker_instance_io_copy) {
	struct t_interlocker_instance_io * const interlocker_instance_io = 
		&dyn_containers_interface->interlocker_instances_io[dyn_containers_interlocker_instance];
	
	pthread_mutex_lock(&dyn_containers_mutex);
	interlocker_instance_io_copy.output_in_use = interlocker_instance_io->output_in_use;
	interlocker_instance_io_copy.output_has_reset = interlocker_instance_io->output_has_reset;
	interlocker_instance_io_copy.output_interlocker_type = interlocker_instance_io->output_interlocker_type;
	strncpy(interlocker_instance_io_copy.output_route_id, interlocker_instance_io->output_route_id, NAME_MAX);
	interlocker_instance_io_copy.output_terminated = interlocker_instance_io->output_terminated;
	pthread_mutex_unlock(&dyn_containers_mutex);
}
