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
 * - Tri Nguyen <https://github.com/trinnguyen>
 *
 */

#ifndef HANDLER_CONTROLLER_H
#define HANDLER_CONTROLLER_H

#include <onion/onion.h>
#include <glib.h>

#define INTERLOCKER_COUNT_MAX           4
#define INTERLOCKER_INSTANCE_COUNT_MAX  4

extern pthread_mutex_t interlocker_mutex;

typedef struct {
	bool is_valid;
	GString *name;
	int dyn_containers_interlocker_instance;
} t_interlocker_data;


void release_all_interlockers(void);

/**
 * Loads the default interlocker
 * @return false if successful, otherwise true
 */
bool load_default_interlocker_instance();

/**
 * Finds conflicting routes that have been granted.
 * 
 * @param route_id id of route for which conflicts should be checked
 * @param include_conflict_train_info whether the train to which a conflicting route is granted
 * shall be added for each element
 * @return GArray of granted route conflicts, described by strings
 */
GArray *get_granted_route_conflicts(const char *route_id, bool include_conflict_train_info);

/**
 * Determines whether a route is physically ready for use:
 * All route signals are in the Stop aspect and all blocks 
 * are unoccupied.
 * 
 * @param ID of route for which clearance should be checked
 * @return true if clear, otherwise false
 */
const bool get_route_is_clear(const char *route_id);

/**
 * Determines whether conflicting routes that have
 * been granted result in route with route_id not 
 * being safe to grant.
 * 
 * @param route_id of route for which conflicts should be checked
 * @return true if route_id is safe to grant under sectional conflict checking view
 * @return false if route_id is not safe to grant under sectional conflict checking view
 */
bool route_has_no_sectional_conflicts(const char *route_id);

/**
  * Finds and grants a requested train route using an external algorithm.
  * A requested route is defined by a pair of source and destination signals. 
  * 
  * @param name of requesting train
  * @param name of the source signal
  * @param name of the destination signal
  * @return ID of the route if it has been granted, otherwise an error string
  */ 
GString *grant_route(const char *train_id, 
                     const char *source_id, 
                     const char *destination_id);

/**
  * Grants a requested train route using an internal algorithm.
  * 
  * @param name of requesting train
  * @param ID of the requested route
  * @return short description of grant success or error
  */ 
const char *grant_route_id(const char *train_id, 
                           const char *route_id);

/**
  * Releases the requested route id.
  * 
  * @param ID of the requested route
  * @return true if the release succeeded
  * @return false if the release failed
  */ 
bool release_route(const char *route_id);

/**
 * Requests the reverser state to be updated and waits
 * for the update to complete. The waiting is bounded by 
 * a maximum number of retries.
 * 
 * @return true if the update was successful, otherwise false
 */
const bool reversers_state_update(void);

onion_connection_status handler_release_route(void *_, onion_request *req,
                                              onion_response *res);

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res);

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res);

onion_connection_status handler_set_peripheral(void *_, onion_request *req,
                                               onion_response *res);

onion_connection_status handler_get_interlocker(void *_, onion_request *req,
                                                onion_response *res);

onion_connection_status handler_set_interlocker(void *_, onion_request *req,
                                                onion_response *res);

onion_connection_status handler_unset_interlocker(void *_, onion_request *req,
                                                  onion_response *res);

#endif  // HANDLER_CONTROLLER_H

