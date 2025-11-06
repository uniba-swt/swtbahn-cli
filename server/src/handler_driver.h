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

#ifndef HANDLER_DRIVER_H
#define HANDLER_DRIVER_H

#include <onion/onion.h>
#include <glib.h>

#define TRAIN_ENGINE_COUNT_MAX			4
#define TRAIN_ENGINE_INSTANCE_COUNT_MAX	5

#define MICROSECOND 1
#define TRAIN_DRIVE_TIME_STEP 	10000 * MICROSECOND		// 0.01 seconds

typedef onion_connection_status o_con_status;

extern pthread_mutex_t grabbed_trains_mutex;

typedef struct {
	bool is_valid;
	GString *name;
	int dyn_containers_engine_instance;
	char track_output[32];
} t_train_data;

extern t_train_data grabbed_trains[TRAIN_ENGINE_INSTANCE_COUNT_MAX];


int train_get_grab_id(const char *train);

bool train_grabbed(const char *train);

bool release_train(int grab_id);

void release_all_grabbed_trains(void);

/**
 * @brief Set the dcc speed for train with id train_id.
 * THIS LOCKS grabbed_trains_mutex, so shall not be called if that is already acquired.
 * If the train is grabbed, sets the speed via the dynamic container/engine.
 * If the train is not grabbed, sets the speed directly via bidib.
 * 
 * @param train_id id of the train
 * @param speed speed in range [0..126]
 * @param req_forwards true for driving forwards, false for driving backwards
 * @param track_output the track output to set the speed for. NULL will use track output "master"
 * @return true parameters valid
 * @return false otherwise.
 */
bool set_dcc_speed_for_train_maybe_grabbed(const char *train_id, int speed, bool req_forwards, const char *track_output);

bool set_dcc_speed_for_grabbed_train(int grab_id, int speed, bool req_forwards, const char *track_output);

/**
 * @brief Stops all trains.
 * For trains that are currently grabbed, sets the speed to 0 via the dynamic container.
 * For trains that are not grabbed, sets the speed to 0 directly via libbidib.
 */
void stop_all_trains();

/**
 * @brief For a given grab-id, return the associated train name (a heap-allocated string) if applicable.
 * Caller shall free the returned string.
 * @param grab_id Grab-id with which the desired train is grabbed
 * @return char* the name of the train grabbed by grab-id; NULL if not grabbed or otherwise invalid.
 */
char *train_id_from_grab_id(int grab_id);

o_con_status handler_grab_train(void *_, onion_request *req, onion_response *res);

o_con_status handler_release_train(void *_, onion_request *req, onion_response *res);

o_con_status handler_request_route(void *_, onion_request *req, onion_response *res);

o_con_status handler_request_route_by_id(void *_, onion_request *req, onion_response *res);

o_con_status handler_driving_direction(void *_, onion_request *req, onion_response *res);

o_con_status handler_drive_route(void *_, onion_request *req, onion_response *res);

o_con_status handler_set_dcc_train_speed(void *_, onion_request *req, onion_response *res);

o_con_status handler_set_calibrated_train_speed(void *_, onion_request *req, onion_response *res);

o_con_status handler_set_train_peripheral(void *_, onion_request *req, onion_response *res);

o_con_status handler_set_train_emergency_stop(void *_, onion_request *req, onion_response *res);


#endif  // HANDLER_DRIVER_H