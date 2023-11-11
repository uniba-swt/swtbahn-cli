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
 * - Nicolas Gross <https://github.com/nicolasgross>
 *
 */

#include <onion/onion.h>
#include <onion/shortcuts.h>
#include <onion/log.h>
#include <onion/low.h>
#include <signal.h>
#include <bidib/bidib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib.h>
#include <time.h>

#include "handler_monitor.h"
#include "handler_admin.h"
#include "handler_driver.h"
#include "handler_controller.h"
#include "handler_upload.h"
#include "websocket_uploader/engine_uploader.h"

#define INPUT_MAX_LEN 256


volatile time_t session_id = 0;
volatile bool running = false;
volatile bool verification_enabled = true;
char serial_device[INPUT_MAX_LEN];
char config_directory[INPUT_MAX_LEN];


void syslog_server(int priority, const char *format, ...) {
	char string[1024];
	va_list arg;
	va_start(arg, format);
	vsnprintf(string, 1024, format, arg);
	
	syslog(priority, "server: %s", string);
}

void build_response_header(onion_response *res) {
	onion_response_set_header(res, "Access-Control-Allow-Origin", 
	                               "*");
	onion_response_set_header(res, "Access-Control-Allow-Headers", 
	                               "Authorization, Origin, X-Requested-With, Content-Type, Accept");
	onion_response_set_header(res, "Access-Control-Allow-Methods", 
	                               "POST, GET, PUT, DELETE, OPTIONS");
}

static onion_connection_status handler_assets(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	onion_response_set_header(res, "Cache-Control", "max-age=43200");
	
	const char local_path[] = "../src/assets/";
	char *global_path = realpath(local_path, NULL);
	if (!global_path) {
		syslog_server(LOG_ERR, "Onion - Cannot calculate the global path of the given directory (%s)",
		              local_path);
		ONION_ERROR("Cannot calculate the global path of the given directory (%s)",
		            local_path);
		return OCS_NOT_IMPLEMENTED;
	}
	
	struct stat st;
	if (stat(global_path, &st) != 0) {
		syslog_server(LOG_ERR, "Onion - Cannot access to the exported directory/file (%s)", 
		              global_path);
		ONION_ERROR("Cannot access to the exported directory/file (%s)", global_path);
		onion_low_free(global_path);
		return OCS_NOT_IMPLEMENTED;
	}
	
	const char *filename = onion_request_get_path(req);
	GString *full_filename = g_string_new(global_path);
	onion_low_free(global_path);
	g_string_append(full_filename, filename);

	onion_connection_status status = 
	    onion_shortcut_response_file(full_filename->str, req, res);
	g_string_free(full_filename, TRUE);
	return status;
}

static int eval_args(int argc, char **argv) {
	if (argc == 5) {
		if (strnlen(argv[1], INPUT_MAX_LEN + 1) == INPUT_MAX_LEN + 1 ||
		    strnlen(argv[2], INPUT_MAX_LEN + 1) == INPUT_MAX_LEN + 1) {
			printf("Serial device and config directory must not exceed %d characters\n",
			       INPUT_MAX_LEN);
			return 1;
		} else if (strnlen(argv[3], 16) == 16) {
			printf("IP address must not exceed 15 characters\n");
			return 1;
		} else if (strnlen(argv[4], 6) == 6) {
			printf("Port must not exceed 5 characters\n");
			return 1;
		} else {
			strcpy(serial_device, argv[1]);
			strcpy(config_directory, argv[2]);
			return 0;
		}
	} else {
		printf("Four arguments expected: <serial device> <config directory> "
			   "<IP address> <port>\n");
		return 1;
	}
}

int main(int argc, char **argv) {
	if (eval_args(argc, argv)) {
		return 1;
	}

	openlog("swtbahn", 0, LOG_LOCAL0);
	syslog_server(LOG_NOTICE, "SWTbahn server started");
	///TODO: Consider making configurable a max_thread count to limit 
	// overloading on weaker setups. Default by onion is 16
	onion *o = onion_new(O_THREADED);
	onion_set_hostname(o, argv[3]);
	onion_set_port(o, argv[4]);
	onion_url *urls = onion_root_url(o);
	
	// --- assets ---
	onion_url_add(urls, "^assets", handler_assets);
	
	// --- home page ---
	onion_url_add_with_data(urls, "", onion_shortcut_internal_redirect, "assets/index.html", NULL);

	// --- admin functions ---
	onion_url_add(urls, "admin/startup", handler_startup);
	onion_url_add(urls, "admin/shutdown", handler_shutdown);
	onion_url_add(urls, "admin/set-track-output", handler_set_track_output);
	onion_url_add(urls, "admin/set-verification-option", handler_set_verification_option);
	onion_url_add(urls, "admin/set-verification-url", handler_set_verification_url);
	onion_url_add(urls, "admin/release-train", handler_admin_release_train);
	onion_url_add(urls, "admin/set-dcc-train-speed", handler_admin_set_dcc_train_speed);
	
	// --- track controller functions ---
	onion_url_add(urls, "controller/release-route", handler_release_route);
	onion_url_add(urls, "controller/set-point", handler_set_point);
	onion_url_add(urls, "controller/set-signal", handler_set_signal);
	onion_url_add(urls, "controller/set-peripheral", handler_set_peripheral);
	onion_url_add(urls, "controller/get-interlocker", handler_get_interlocker);
	onion_url_add(urls, "controller/set-interlocker", handler_set_interlocker);
	onion_url_add(urls, "controller/unset-interlocker", handler_unset_interlocker);
	
	// --- train driver functions ---
	onion_url_add(urls, "driver/grab-train", handler_grab_train);
	onion_url_add(urls, "driver/release-train", handler_release_train);	
	onion_url_add(urls, "driver/request-route", handler_request_route);
	onion_url_add(urls, "driver/request-route-id", handler_request_route_id);
	onion_url_add(urls, "driver/direction", handler_driving_direction);
	onion_url_add(urls, "driver/drive-route", handler_drive_route);
	onion_url_add(urls, "driver/set-dcc-train-speed", handler_set_dcc_train_speed);
	onion_url_add(urls, "driver/set-calibrated-train-speed",
	              handler_set_calibrated_train_speed);
	onion_url_add(urls, "driver/set-train-emergency-stop",
	              handler_set_train_emergency_stop);
	onion_url_add(urls, "driver/set-train-peripheral",
	              handler_set_train_peripheral);

	// --- upload functions ---
	onion_url_add(urls, "upload/engine", handler_upload_engine);
	onion_url_add(urls, "upload/refresh-engines", handler_refresh_engines);
	onion_url_add(urls, "upload/remove-engine", handler_remove_engine);
	onion_url_add(urls, "upload/interlocker", handler_upload_interlocker);
	onion_url_add(urls, "upload/refresh-interlockers", handler_refresh_interlockers);
	onion_url_add(urls, "upload/remove-interlocker", handler_remove_interlocker);

	// --- monitor functions ---
	onion_url_add(urls, "monitor/trains", handler_get_trains);
	onion_url_add(urls, "monitor/train-state", handler_get_train_state);
	onion_url_add(urls, "monitor/train-peripherals", handler_get_train_peripherals);
	onion_url_add(urls, "monitor/track-outputs", handler_get_track_outputs);
	onion_url_add(urls, "monitor/points", handler_get_points);
	onion_url_add(urls, "monitor/signals", handler_get_signals);
	onion_url_add(urls, "monitor/point-aspects", handler_get_point_aspects);
	onion_url_add(urls, "monitor/signal-aspects", handler_get_signal_aspects);
	onion_url_add(urls, "monitor/segments", handler_get_segments);
	onion_url_add(urls, "monitor/reversers", handler_get_reversers);
	onion_url_add(urls, "monitor/peripherals", handler_get_peripherals);
	onion_url_add(urls, "monitor/verification-option", handler_get_verification_option);
	onion_url_add(urls, "monitor/verification-url", handler_get_verification_url);
	onion_url_add(urls, "monitor/granted-routes", handler_get_granted_routes);
	onion_url_add(urls, "monitor/route", handler_get_route);
	onion_url_add(urls, "monitor/debug", handler_get_debug_info);
	onion_url_add(urls, "monitor/debug_extra", handler_get_debug_info_extra);
	
	load_cached_verifier_url();

	onion_listen(o);
	onion_free(o);
	if (running) {
		stop_bidib();
	}
	cache_verifier_url();
	free_verifier_url();
	
	syslog_server(LOG_NOTICE, "%s", "SWTbahn server stopped");
	closelog();

	return 0;
}

