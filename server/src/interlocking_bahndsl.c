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
 * - Tri Nguyen <https://github.com/trinnguyen>
 *
 */

#include "dynlib.h"
#include "tick_data.h"
#include "server.h"
#include "interlocking.h"
#include "bahn_data_util.h"

#include "interlocking_bahndsl.h"

pthread_mutex_t interlocker_bahndsl_mutex = PTHREAD_MUTEX_INITIALIZER;

static dynlib_data lib_interlocking = {};

int load_interlocking_library() {
    const char *path = "../src/interlockers/libinterlocker_default";
    dynlib_status status = dynlib_load(&lib_interlocking, path, INTERLOCKER);
    if (status == DYNLIB_LOAD_SUCCESS) {
        syslog_server(LOG_NOTICE, "Loaded dynamic interlocking library: %s", path);
        return 0;
    }

    return 1;
}

void close_interlocking_library() {
    dynlib_close(&lib_interlocking);
}

char *grant_route_with_bahndsl(const char *train_id, const char *source_id, const char *destination_id) {
    pthread_mutex_lock(&interlocker_bahndsl_mutex);

    // init cache
    init_cached_track_state();

    // request route
    request_route_tick_data tick_data = {.src_signal_id = strdup(source_id),
            .dst_signal_id = strdup(destination_id),
            .train_id = strdup(train_id)};

    lib_interlocking.request_reset_func(&tick_data);
    while (tick_data.terminated == 0) {
        lib_interlocking.request_tick_func(&tick_data);
    }

    // Free
    free(tick_data.src_signal_id);
    free(tick_data.dst_signal_id);
    free(tick_data.train_id);
    free_cached_track_state();

    // result
    char *route_id = tick_data.out;
    if (route_id != NULL) {
        syslog_server(LOG_NOTICE, "Grant route with algorithm: Route %s has been granted", route_id);
    } else {
        syslog_server(LOG_ERR, "Grant route with algorithm: Route could not be granted");
    }

    pthread_mutex_unlock(&interlocker_bahndsl_mutex);
    return route_id;
}
