/*
 *
 * Copyright (C) 2023 University of Bamberg, Software Technologies Research Group
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

#ifndef DYN_CONTAINERS_H
#define DYN_CONTAINERS_H

#include <pthread.h>
#include <stdbool.h>
#include <limits.h>

// Copy of the data structures defined in the generated C code of dyn_containers.forec.

typedef int bool__global_0_0;

#define bool__global_0_0 bool

typedef struct {
	volatile bool__global_0_0 terminate;
	int let_period_us;
} t_forec_intern_input__global_0_0;

typedef struct {
	/* shared */ t_forec_intern_input__global_0_0 value;
	int status;
} Shared_forec_intern_input__global_0_0;

extern Shared_forec_intern_input__global_0_0 forec_intern_input__global_0_0;


typedef struct {
	volatile bool__global_0_0 load;
	volatile bool__global_0_0 unload;
	char filepath[PATH_MAX + NAME_MAX];
} t_forec_intern_input_train_engine__global_0_0;

typedef struct {
	/* shared */ t_forec_intern_input_train_engine__global_0_0 value;
	int status;
} Shared_forec_intern_input_train_engine_0__global_0_0;

extern Shared_forec_intern_input_train_engine_0__global_0_0
	forec_intern_input_train_engine_0__global_0_0,
	forec_intern_input_train_engine_1__global_0_0,
	forec_intern_input_train_engine_2__global_0_0,
	forec_intern_input_train_engine_3__global_0_0;


typedef struct {
	volatile bool__global_0_0 in_use;
	char *name;
} t_forec_intern_output_train_engine__global_0_0;

typedef struct {
	/* shared */ t_forec_intern_output_train_engine__global_0_0 value;
	int status;
} Shared_forec_intern_output_train_engine_0__global_0_0;

extern Shared_forec_intern_output_train_engine_0__global_0_0
	forec_intern_output_train_engine_0__global_0_0,
	forec_intern_output_train_engine_1__global_0_0,
	forec_intern_output_train_engine_2__global_0_0,
	forec_intern_output_train_engine_3__global_0_0;


typedef struct {
	volatile bool__global_0_0 grab;
	volatile bool__global_0_0 release;
	int train_engine_type;
	int requested_speed;
	char requested_forwards;
} t_forec_intern_input_train_engine_instance__global_0_0;

typedef struct {
	/* shared */ t_forec_intern_input_train_engine_instance__global_0_0 value;
	int status;
} Shared_forec_intern_input_train_engine_instance_0__global_0_0;

extern Shared_forec_intern_input_train_engine_instance_0__global_0_0
	forec_intern_input_train_engine_instance_0__global_0_0,
	forec_intern_input_train_engine_instance_1__global_0_0,
	forec_intern_input_train_engine_instance_2__global_0_0,
	forec_intern_input_train_engine_instance_3__global_0_0,
	forec_intern_input_train_engine_instance_4__global_0_0;


typedef struct {
	volatile bool__global_0_0 in_use;
	int train_engine_type;
	int nominal_speed;
	char nominal_forwards;
} t_forec_intern_output_train_engine_instance__global_0_0;

typedef struct {
	/* shared */ t_forec_intern_output_train_engine_instance__global_0_0 value;
	int status;
} Shared_forec_intern_output_train_engine_instance_0__global_0_0;

extern Shared_forec_intern_output_train_engine_instance_0__global_0_0
	forec_intern_output_train_engine_instance_0__global_0_0,
	forec_intern_output_train_engine_instance_1__global_0_0,
	forec_intern_output_train_engine_instance_2__global_0_0,
	forec_intern_output_train_engine_instance_3__global_0_0,
	forec_intern_output_train_engine_instance_4__global_0_0;


typedef enum {
	// PAR
	FOREC_PAR_OFF,				// 0
	FOREC_PAR_ON,				// 1

	// Core
	FOREC_CORE_REACTING,		// 2
	FOREC_CORE_REACTED,			// 3
	FOREC_CORE_TERMINATED,		// 4
	
	// Shared variables
	FOREC_SHARED_UNMODIFIED,	// 5
	FOREC_SHARED_MODIFIED,		// 6
	FOREC_SHARED_WAS_MODIFIED,	// 7
	
	// Program termination
	RUNNING,					// 8
	TERMINATED					// 9
} Status;

typedef struct {
	volatile int sync;
	volatile int activeThreads;
	volatile Status status;
	pthread_cond_t statusCond;
	pthread_mutex_t statusLock;
	volatile int reactionCounter;
	pthread_cond_t reactionCounterCond;
	pthread_mutex_t reactionCounterLock;
} Core;

extern Core mainParCore0;
extern Core mainParCore1;
extern Core mainParCore2;

extern int mainParReactionCounter;

#endif  // DYN_CONTAINERS_H
