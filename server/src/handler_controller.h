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

#ifndef SWTSERVER_HANDLER_CONTROLLER_H
#define SWTSERVER_HANDLER_CONTROLLER_H

extern pthread_mutex_t interlocker_mutex;

bool route_is_unavailable_or_conflicted(const int route_id);
bool route_is_clear(const int route_id, const char *train_id);
bool set_route_points_signals(const int route_id);
bool block_route(const int route_id, const char *train_id);

/**
  * Finds and grants a requested train route.
  * A requested route is defined by a pair of source and destination signals. 
  * 
  * @param name of requesting train
  * @param name of the source signal
  * @param name of the destination signal
  * @return ID of the route if it has been granted, otherwise -1
  */ 
int grant_route_with_algorithm(const char *train_id, const char *source_id, 
                               const char *destination_id);

void release_route(const int route_id);

onion_connection_status handler_release_route(void *_, onion_request *req,
                                              onion_response *res);

onion_connection_status handler_set_point(void *_, onion_request *req,
                                          onion_response *res);

onion_connection_status handler_set_signal(void *_, onion_request *req,
                                           onion_response *res);


#endif

