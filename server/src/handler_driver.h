/*
 *
 * Copyright (C) 2017 University of Bamberg, Software Technologies Research Group
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
 * - Nicolas Gross <https://github.com/nicolasgross>
 *
 */

#ifndef SWTSERVER_HANDLER_DRIVER_H
#define SWTSERVER_HANDLER_DRIVER_H

#include <onion/onion.h>
#include <glib.h>

#define TRAIN_ENGINE_COUNT_MAX			4
#define TRAIN_ENGINE_INSTANCE_COUNT_MAX	5

extern pthread_mutex_t grabbed_trains_mutex;

typedef struct {
	bool is_valid;
	GString *name;
	int dyn_containers_engine_instance;
	char track_output[32];
} t_train_data;

extern t_train_data grabbed_trains[TRAIN_ENGINE_INSTANCE_COUNT_MAX];


const int train_get_grab_id(const char *train);

bool train_grabbed(const char *train);

void free_all_grabbed_trains(void);

onion_connection_status handler_grab_train(void *_, onion_request *req,
                                           onion_response *res);

onion_connection_status handler_release_train(void *_, onion_request *req,
                                              onion_response *res);

onion_connection_status handler_request_route(void *_, onion_request *req,
                                              onion_response *res);

onion_connection_status handler_request_route_id(void *_, onion_request *req,
                                                 onion_response *res);

onion_connection_status handler_drive_route(void *_, onion_request *req,
                                            onion_response *res);

onion_connection_status handler_set_dcc_train_speed(void *_, onion_request *req,
                                                    onion_response *res);

onion_connection_status handler_set_calibrated_train_speed(void *_,
                                                           onion_request *req,
                                                           onion_response *res);

onion_connection_status handler_set_train_peripheral(void *_, onion_request *req,
                                                     onion_response *res);

onion_connection_status handler_set_train_emergency_stop(void *_, onion_request *req,
                                                         onion_response *res);


#endif

