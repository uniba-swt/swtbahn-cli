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

#ifndef SWTSERVER_HANDLER_CONTROLLER_H
#define SWTSERVER_HANDLER_CONTROLLER_H

extern pthread_mutex_t interlocker_mutex;

/**
 * Load dynamic interlocker compiled by BahnDSL
 * File: interlocker/libinterlocker_default
 * Should being called once in the application lifetimme
 * @return 1 if successful, otherwise 0
 */
int load_interlocker_default();

/**
 * Close dynamic interlocker compiled by BahnDSL
 */
void close_interlocker_default();

/**
  * Finds and grants a requested train route.
  * A requested route is defined by a pair of source and destination signals. 
  * 
  * @param name of requesting train
  * @param name of the source signal
  * @param name of the destination signal
  * @return ID of the route if it has been granted, otherwise NULL
  */ 
char *grant_route(const char *train_id, const char *source_id, const char *destination_id);

void release_route(const int route_id);

onion_connection_status handler_release_route(void *_, onion_request *req,
                                              onion_response *res);

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res);

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res);

onion_connection_status handler_set_interlocker(void *_, onion_request *req,
                                                onion_response *res);

onion_connection_status handler_unset_interlocker(void *_, onion_request *req,
                                                  onion_response *res);

#endif	// SWTSERVER_HANDLER_CONTROLLER_H

