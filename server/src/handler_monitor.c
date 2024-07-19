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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 *
 */

#include <bidib/bidib.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>

#include "handler_monitor.h"
#include "server.h"
#include "handler_controller.h"
#include "handler_driver.h"
#include "param_verification.h"
#include "interlocking.h"
#include "bahn_data_util.h"
#include "websocket_uploader/engine_uploader.h"
#include "json_response_builder.h"
#include "communication_utils.h"

///NOTE: All handlers/endpoints now only accept GET, not POST. Need to adjust clients accordingly.

static GArray* garray_points_to_garray_str_ids(GArray *points) {
	if (points == NULL) {
		return NULL;
	}
	GArray *g_point_ids = g_array_new(FALSE, FALSE, sizeof(char *));
	
	for (int i = 0; i < points->len; ++i) {
		t_interlocking_point *point = &g_array_index(points, t_interlocking_point, i);
		if (point != NULL) {
			char *point_id_dcopy = strdup(point->id);
			if (point_id_dcopy != NULL) {
				g_array_append_val(g_point_ids, point_id_dcopy);
			}
		}
	}
	return g_point_ids;
}

static void free_g_strarray(GArray *g_strarray) {
	if (g_strarray == NULL) {
		return;
	}
	for (int i = 0; i < g_strarray->len; ++i) {
		if (g_array_index(g_strarray, char *, i) != NULL) {
			free(g_array_index(g_strarray, char *, i));
		}
	}
	g_array_free(g_strarray, true);
	g_strarray = NULL;
}

static GString *get_trains_json() {
	t_bidib_id_list_query query = bidib_get_trains();
	GString *g_trains = g_string_sized_new(60 * (query.length + 1));
	g_string_assign(g_trains, "");
	
	append_start_of_obj(g_trains, false);
	append_field_start_of_list(g_trains, "trains");
	
	for (size_t i = 0; i < query.length; i++) {
		append_start_of_obj(g_trains, true);
		
		append_field_str_value(g_trains, "id", query.ids[i], true);
		append_field_bool_value(g_trains, "grabbed", train_grabbed(query.ids[i]), true);
		
		t_bidib_train_position_query train_position_query = bidib_get_train_position(query.ids[i]);
		append_field_bool_value(g_trains, "on_track", train_position_query.length > 0, false);
		bidib_free_train_position_query(train_position_query);
		
		append_end_of_obj(g_trains, i+1 < query.length);
	}
	append_end_of_list(g_trains, false, query.length > 0);
	append_end_of_obj(g_trains, false);
	bidib_free_id_list_query(query);
	return g_trains;
}

o_con_status handler_get_trains(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_trains = get_trains_json();
		send_some_gstring_and_free(res, HTTP_OK, g_trains);
		syslog_server(LOG_INFO, "Request: Get available trains - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "");
	}
}

static GString *get_train_state_json(const char *data_train) {
	if (data_train == NULL) {
		return NULL;
	}
	
	t_bidib_train_state_query train_state_query = bidib_get_train_state(data_train);
	if (!train_state_query.known) {
		bidib_free_train_state_query(train_state_query);
		return NULL;
	}
	
	GString *g_train_state = g_string_sized_new(256);
	g_string_assign(g_train_state, "");
	append_start_of_obj(g_train_state, false);
	append_field_str_value(g_train_state, "id", data_train, true);
	append_field_bool_value(g_train_state, "grabbed", train_grabbed(data_train), true);
	const char *orientation_str = (train_state_query.data.orientation ==
			                       BIDIB_TRAIN_ORIENTATION_LEFT) ?
			                       "left" : "right";
	append_field_str_value(g_train_state, "orientation", orientation_str, true);
	const char *direction_str = train_state_query.data.set_is_forwards ? "forwards" : "backwards";
	append_field_str_value(g_train_state, "direction", direction_str, true);
	
	append_field_int_value(g_train_state, "speed_step", train_state_query.data.set_speed_step, true);
	append_field_int_value(g_train_state, "detected_kmh_speed", train_state_query.data.detected_kmh_speed, true);
	
	bidib_free_train_state_query(train_state_query);
	
	pthread_mutex_lock(&interlocker_mutex);
	int route_id = interlocking_table_get_route_id_of_train(data_train);
	pthread_mutex_unlock(&interlocker_mutex);
	append_field_int_value(g_train_state, "route_id", route_id, true);
	
	t_bidib_train_position_query train_position_query = bidib_get_train_position(data_train);
	bool is_on_track = train_position_query.length > 0;
	// is_on_track intentionally used for both value and add_trailing_comma parameters.
	append_field_bool_value(g_train_state, "on_track", is_on_track, is_on_track);
	if (is_on_track) {
		// two passes likely needed, one for segments, one for blocks.
		append_field_strlist_value(g_train_state, "occupied_segments", 
		                          (const char **) train_position_query.segments, 
		                          train_position_query.length, true);
		
		append_field_start_of_list(g_train_state, "occupied_blocks");
		int added_blocks = 0;
		for (size_t i = 0; i < train_position_query.length; i++) {
			const char *block_id = config_get_block_id_of_segment(train_position_query.segments[i]);
			// only add block if it does not already exist in the list (and thus in g_train_state)
			if (block_id != NULL && strstr(g_train_state->str, block_id) == NULL) {
				g_string_append_printf(g_train_state, "%s\"%s\"", added_blocks > 0 ? ", " : "", block_id);
				++added_blocks;
			}
		}
		append_end_of_list(g_train_state, false, false);
	}
	
	bidib_free_train_position_query(train_position_query);
	append_end_of_obj(g_train_state, false);
	return g_train_state;
}

o_con_status handler_get_train_state(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train state - invalid parameters");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		} 
		
		GString *ret_string = get_train_state_json(data_train);
		if (ret_string != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, ret_string);
			syslog_server(LOG_INFO, "Request: Get train state - train: %s - done", data_train);
		} else {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, 
			              "Request: Get train state - train: %s - invalid train", 
			              data_train);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get train state");
	}
}

static GString *get_train_states_json() {
	t_bidib_id_list_query query = bidib_get_trains();
	GString *g_train_states = g_string_sized_new(256 * (query.length + 1));
	g_string_assign(g_train_states, "");
	
	append_start_of_obj(g_train_states, false);
	append_field_start_of_list(g_train_states, "train-states");
	
	for (size_t i = 0; i < query.length; i++) {
		GString *g_train_state = get_train_state_json(query.ids[i]);
		if (g_train_state != NULL) {
			g_string_append_printf(g_train_states, "%s", g_train_state->str);
			g_string_free(g_train_state, true);
			if (i + 1 < query.length) {
				g_string_append_printf(g_train_states, "%s", ",");
			}
		}
	}
	append_end_of_list(g_train_states, false, query.length > 0);
	append_end_of_obj(g_train_states, false);
	
	bidib_free_id_list_query(query);
	return g_train_states;
}

o_con_status handler_get_train_states(void *_, onion_request *req, onion_response *res) {
    build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_train_states = get_train_states_json();
		send_some_gstring_and_free(res, HTTP_OK, g_train_states);
		syslog_server(LOG_INFO, "Request: Get train states - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get train states");
	}
}

static GString *get_train_peripherals_json(const char *data_train) {
	if (data_train == NULL) {
		return NULL;
	}
	
	t_bidib_id_list_query query = bidib_get_train_peripherals(data_train);
	GString *g_train_peripherals = g_string_sized_new(40 * (query.length + 1));
	g_string_assign(g_train_peripherals, "");
	
	append_start_of_obj(g_train_peripherals, false);
	append_field_start_of_list(g_train_peripherals, "peripherals");
	
	for (size_t i = 0; i < query.length; i++) {
		t_bidib_train_peripheral_state_query train_peripheral_state =
				bidib_get_train_peripheral_state(data_train, query.ids[i]);
		char *state_string;
		if (train_peripheral_state.available) {
			state_string = train_peripheral_state.state == 1 ? "on" : "off";
		} else {
			state_string = "unknown";
		}
		
		append_start_of_obj(g_train_peripherals, true);
		append_field_str_value(g_train_peripherals, "id", query.ids[i], true);
		append_field_str_value(g_train_peripherals, "state", state_string, false);
		append_end_of_obj(g_train_peripherals, i+1 < query.length);
	}
	
	append_end_of_list(g_train_peripherals, false, query.length > 0);
	append_end_of_obj(g_train_peripherals, false);
	
	bidib_free_id_list_query(query);
	return g_train_peripherals;
}

o_con_status handler_get_train_peripherals(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train peripherals - invalid parameters");
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			return OCS_PROCESSED;
		}
		
		GString *g_train_peripherals = get_train_peripherals_json(data_train);
		if (g_train_peripherals != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, g_train_peripherals);
			syslog_server(LOG_INFO, 
			              "Request: Get train peripherals - train: %s - done",
			              data_train);
		} else {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, 
			              "Request: Get train peripherals - train: %s - invalid train",
			              data_train);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get train peripherals");
	}
}

static GString *get_engines_or_interlockers_json(bool engines) {
	GString *g_json_ret = g_string_new("");
	if (g_json_ret == NULL) {
		syslog_server(LOG_ERR, "Get engines or interlockers json - can't allocate g_engines");
		return NULL;
	}
	append_start_of_obj(g_json_ret, false);
	GArray *names_list = NULL;
	if (engines) {
		names_list = dyn_containers_get_train_engines_arr();
	} else {
		names_list = dyn_containers_get_interlockers_arr();
	}
	if (names_list == NULL) {
		syslog_server(LOG_ERR, "Get engines or interlockers json - list from dyncontainers is NULL");
		return NULL;
	}
	const char *fieldname = engines ? "engines" : "interlockers";
	append_field_strlist_value_garray_strs(g_json_ret, fieldname, names_list, false);
	append_end_of_obj(g_json_ret, false);
	free_g_strarray(names_list);
	return g_json_ret;
}

static o_con_status get_engines_interlockers_common(onion_request *req, onion_response *res, 
                                                    bool engines) {
	build_response_header(res);
	const char *l_name = engines ? "Get engines" : "Get interlockers";
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *g_ret = get_engines_or_interlockers_json(engines);
		if (g_ret != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, g_ret);
		} else {
			onion_response_set_code(res, HTTP_INTERNAL_ERROR);
		}
		syslog_server(LOG_INFO, "Request: %s - done", l_name);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, l_name);
	}
}

o_con_status handler_get_engines(void *_, onion_request *req, onion_response *res) {
	return get_engines_interlockers_common(req, res, true);
}

o_con_status handler_get_interlockers(void *_, onion_request *req, onion_response *res) {
	return get_engines_interlockers_common(req, res, false);
}

static GString *get_track_outputs_json() {
	t_bidib_id_list_query query = bidib_get_track_outputs();
	GString *g_track_outputs = g_string_sized_new(48 * (query.length + 1));
	
	g_string_assign(g_track_outputs, "");
	append_start_of_obj(g_track_outputs, false);
	append_field_start_of_list(g_track_outputs, "track-outputs");
	
	int track_outputs_added = 0;
	for (size_t i = 0; i < query.length; i++) {
		t_bidib_track_output_state_query track_output_state_query = 
				bidib_get_track_output_state(query.ids[i]);
		if (track_output_state_query.known) {
			char *state_string;
			switch (track_output_state_query.cs_state) {
				case 0x00: state_string = "off"; break;
				case 0x01: state_string = "stop"; break;
				case 0x02: state_string = "soft stop"; break;
				case 0x03: state_string = "go"; break;
				case 0x04: state_string = "go + ignore watchdog"; break;
				case 0x08: state_string = "prog"; break;
				case 0x09: state_string = "prog busy"; break;
				case 0x0D: state_string = "busy"; break;
				case 0xFF: state_string = "query"; break;
				default:   state_string = "off"; break;
			}
			if (track_outputs_added > 0) {
				g_string_append_c(g_track_outputs, ',');
			}
			track_outputs_added++;
			append_start_of_obj(g_track_outputs, true);
			append_field_str_value(g_track_outputs, "id", query.ids[i], true);
			append_field_str_value(g_track_outputs, "state", state_string, false);
			append_end_of_obj(g_track_outputs, false);
		}
	}
	append_end_of_list(g_track_outputs, false, track_outputs_added > 0);
	append_end_of_obj(g_track_outputs, false);
	bidib_free_id_list_query(query);
	return g_track_outputs;
}

o_con_status handler_get_track_outputs(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_track_outputs = get_track_outputs_json();
		send_some_gstring_and_free(res, HTTP_OK, g_track_outputs);
		syslog_server(LOG_INFO, "Request: Get track outputs - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get track outputs");
	}
}

static GString *get_accessories_json(bool point_accessories) {
	t_bidib_id_list_query query;
	if (point_accessories) {
		query = bidib_get_connected_points();
	} else {
		query = bidib_get_connected_signals();
	}
	GString *g_accs = g_string_sized_new(70 * (query.length + 1));
	g_string_assign(g_accs, "");
	append_start_of_obj(g_accs, false);
	append_field_start_of_list(g_accs, point_accessories ? "points" : "signals");
	
	for (size_t i = 0; i < query.length; i++) {
		t_bidib_unified_accessory_state_query acc_state = 
				point_accessories ? bidib_get_point_state(query.ids[i]) : bidib_get_signal_state(query.ids[i]);
		
		append_start_of_obj(g_accs, true);
		append_field_str_value(g_accs, "id", query.ids[i], true);
		append_field_str_value(g_accs, "state", 
		                       acc_state.type == BIDIB_ACCESSORY_BOARD ?
		                       acc_state.board_accessory_state.state_id :
		                       acc_state.dcc_accessory_state.state_id, 
		                       (!point_accessories 
		                           || (point_accessories && acc_state.type == BIDIB_ACCESSORY_BOARD)));
		if (point_accessories && acc_state.type == BIDIB_ACCESSORY_BOARD) {
			append_field_bool_value(g_accs, "target_state_reached", 
			                        acc_state.board_accessory_state.execution_state, false);
		} else if (!point_accessories) {
			append_field_str_value(g_accs, "type", 
		                           config_get_scalar_string_value("signal", query.ids[i], "type"), 
		                           false);
		}
		append_end_of_obj(g_accs, i+1 < query.length);
		bidib_free_unified_accessory_state_query(acc_state);
	}
	append_end_of_list(g_accs, false, query.length > 0);
	append_end_of_obj(g_accs, false);
	bidib_free_id_list_query(query);
	return g_accs;
}

static o_con_status get_points_signals_common(onion_request *req, onion_response *res, bool points) {
	build_response_header(res);
	const char *l_name = points ? "Get points" : "Get signals";
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_ret = get_accessories_json(points);
		send_some_gstring_and_free(res, HTTP_OK, g_ret);
		syslog_server(LOG_INFO, "Request: %s - done", l_name);
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, l_name);
	}
}

o_con_status handler_get_points(void *_, onion_request *req, onion_response *res) {
	return get_points_signals_common(req, res, true);
}

o_con_status handler_get_signals(void *_, onion_request *req, onion_response *res) {
	return get_points_signals_common(req, res, false);
}

static GString *get_point_details_json(const char *data_id) {
	if (data_id == NULL) {
		return NULL;
	}
	
	t_bidib_id_list_query aspects_query = bidib_get_point_aspects(data_id);
	if (aspects_query.length <= 0 || aspects_query.ids == NULL) {
		return NULL;
	}
	t_bidib_unified_accessory_state_query acc_state = bidib_get_point_state(data_id);
	if (!acc_state.known) {
		bidib_free_id_list_query(aspects_query);
		return NULL;
	}
	
	GString *g_details = g_string_sized_new(156);
	g_string_assign(g_details, "");
	append_start_of_obj(g_details, false);
	append_field_str_value(g_details, "id", data_id, true);
	append_field_start_of_list(g_details, "aspects");
	
	for (size_t i = 0; i < aspects_query.length; i++) {
		g_string_append_printf(g_details, "%s\"%s\"", i != 0 ? ", " : "", aspects_query.ids[i]);
	}
	bidib_free_id_list_query(aspects_query);
	
	append_end_of_list(g_details, true, false);
	append_field_str_value(g_details, "state", 
	                       acc_state.type == BIDIB_ACCESSORY_BOARD ?
	                       acc_state.board_accessory_state.state_id :
	                       acc_state.dcc_accessory_state.state_id, 
	                       true);
	const char *point_segment = config_get_scalar_string_value("point", data_id, "segment");
	append_field_str_value(g_details, "segment", point_segment, true);
	
	append_field_bool_value(g_details, "occupied", 
	                        is_segment_occupied(point_segment), 
	                        acc_state.type == BIDIB_ACCESSORY_BOARD);
	
	if (acc_state.type == BIDIB_ACCESSORY_BOARD) {
		append_field_bool_value(g_details, "target_state_reached", 
		                        acc_state.board_accessory_state.execution_state, false);
	}
	
	append_end_of_obj(g_details, false);
	return g_details;
}

o_con_status handler_get_point_details(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		const char *data_point = onion_request_get_post(req, "point");
		if (data_point == NULL) {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, "Request: Get point details - invalid parameters");
			return OCS_PROCESSED;
		}
		
		GString *g_details = get_point_details_json(data_point);
		if (g_details != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, g_details);
			syslog_server(LOG_INFO, "Request: Get point details - point: %s - done", data_point);
		} else {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, 
			              "Request: Get point details - point: %s - invalid point", 
			              data_point);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get point details");
	}
}


static GString *get_accessory_aspects_json(const char *data_id, bool is_point) {
	if (data_id == NULL) {
		return NULL;
	}
	t_bidib_id_list_query query;
	if (is_point) {
		query = bidib_get_point_aspects(data_id);
	} else {
		query = bidib_get_signal_aspects(data_id);
	}
	
	if (query.length <= 0 || query.ids == NULL) {
		return NULL;
	}
	
	GString *g_aspects = g_string_sized_new(32 * (query.length + 1));
	g_string_assign(g_aspects, "");
	append_start_of_obj(g_aspects, false);
	append_field_start_of_list(g_aspects, "aspects");
	
	for (size_t i = 0; i < query.length; i++) {
		g_string_append_printf(g_aspects, "%s\"%s\"", i != 0 ? ", " : "", query.ids[i]);
	}
	append_end_of_list(g_aspects, false, false);
	append_end_of_obj(g_aspects, false);
	bidib_free_id_list_query(query);
	return g_aspects;
}

static o_con_status get_acc_aspects_common(onion_request *req, onion_response *res, bool point) {
	build_response_header(res);
	const char *l_name = point ? "Get point aspects" : "Get signal aspects";
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		const char *acc_type_name = point ? "point" : "signal";
		const char *data_acc = onion_request_get_post(req, acc_type_name);
		if (data_acc == NULL) {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, "Request: %s - invalid parameters", l_name);
			return OCS_PROCESSED;
		}
		
		GString *g_aspects = get_accessory_aspects_json(data_acc, point);
		if (g_aspects != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, g_aspects);
			syslog_server(LOG_INFO, "Request: %s - %s: %s - done", l_name, acc_type_name, data_acc);
		} else {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, 
			              "Request: %s - %s: %s - invalid %s", 
			               l_name, acc_type_name, data_acc, acc_type_name);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, l_name);
	}
}

o_con_status handler_get_point_aspects(void *_, onion_request *req, onion_response *res) {
	return get_acc_aspects_common(req, res, true);
}

// Probably want to have a signal-details endpoint too, once we 
// have more information about signals we can return; at the moment it
// is quite limited. One interesting property may be, for example,
// to know what segment->segment travel means the signal has been driven past.
o_con_status handler_get_signal_aspects(void *_, onion_request *req, onion_response *res) {
	return get_acc_aspects_common(req, res, false);
}

static GString *get_segments_json() {
	t_bidib_id_list_query seg_query = bidib_get_connected_segments();
	// no check for empty query, as then the code here constructs a json with an
	// empty list, which is good -> better tells the client that no
	// segments are known than a totally empty (i.e. empty string) answer.
	
	GString *g_segments = g_string_sized_new(42 * (seg_query.length + 1));
	g_string_assign(g_segments, "");
	append_start_of_obj(g_segments, false);
	append_field_start_of_list(g_segments, "segments");
	
	int added_segments = 0;
	for (size_t i = 0; i < seg_query.length; i++) {
		t_bidib_segment_state_query seg_state_query = bidib_get_segment_state(seg_query.ids[i]);
		if (added_segments++ > 0) {
			g_string_append_c(g_segments, ',');
		}
		append_start_of_obj(g_segments, true);
		append_field_str_value(g_segments, "id", seg_query.ids[i], true);
		
		append_field_start_of_list(g_segments, "occupied-by");
		for (size_t j = 0; j < seg_state_query.data.dcc_address_cnt; j++) {
			t_bidib_id_query id_query = bidib_get_train_id(seg_state_query.data.dcc_addresses[j]);
			
			g_string_append_printf(g_segments, "%s\"%s\"", 
			                       j != 0 ? ", " : "", 
			                       id_query.known ? id_query.id : "unknown");
			bidib_free_id_query(id_query);
		}
		
		///TODO: Test difference
		//append_end_of_list(g_segments, false, seg_state_query.data.dcc_address_cnt > 0);
		append_end_of_list(g_segments, false, false);
		append_end_of_obj(g_segments, false);
		
		bidib_free_segment_state_query(seg_state_query);
		
	}
	append_end_of_list(g_segments, false, added_segments > 0);
	append_end_of_obj(g_segments, false);
	bidib_free_id_list_query(seg_query);
	return g_segments;
}

o_con_status handler_get_segments(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_segments = get_segments_json();
		send_some_gstring_and_free(res, HTTP_OK, g_segments);
		syslog_server(LOG_INFO, "Request: Get segments - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get segments");
	}
}

static GString *get_reversers_json() {
	t_bidib_id_list_query rev_query = bidib_get_connected_reversers();
	GString *g_reversers = g_string_sized_new(48 * (rev_query.length + 1));
	g_string_assign(g_reversers, "");
	
	append_start_of_obj(g_reversers, false);
	append_field_start_of_list(g_reversers, "reversers");
	
	int added_reversers = 0;
	for (size_t i = 0; i < rev_query.length; i++) {
		const char *reverser_id = rev_query.ids[i];
		t_bidib_reverser_state_query rev_state_query = bidib_get_reverser_state(reverser_id);
		if (!rev_state_query.available) {
			bidib_free_reverser_state_query(rev_state_query);
			continue;
		}
		
		char *state_value_str = "unknown";
		switch (rev_state_query.data.state_value) {
			case BIDIB_REV_EXEC_STATE_OFF: 
				state_value_str = "off";
				break;
			case BIDIB_REV_EXEC_STATE_ON: 
				state_value_str = "on";
				break;
			default:
				state_value_str = "unknown";
				break;
		}
		bidib_free_reverser_state_query(rev_state_query);
		
		if (added_reversers++ > 0) {
			g_string_append_c(g_reversers, ',');
		}
		append_start_of_obj(g_reversers, true);
		append_field_str_value(g_reversers, "id", reverser_id, true);
		append_field_str_value(g_reversers, "state", state_value_str, false);
		append_end_of_obj(g_reversers, false);
		
	}
	append_end_of_list(g_reversers, false, added_reversers > 0);
	append_end_of_obj(g_reversers, false);
	bidib_free_id_list_query(rev_query);
	return g_reversers;
}

o_con_status handler_get_reversers(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		if (!reversers_state_update()) {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, "Request: Get reversers - unable to request state update");
			return OCS_PROCESSED;
		}
		GString *g_reversers = get_reversers_json();
		send_some_gstring_and_free(res, HTTP_OK, g_reversers);
		syslog_server(LOG_INFO, "Request: Get reversers - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get reversers");
	}
}

static GString *get_peripherals_json() {
	// Trying out ways of estimating size. The +1 is there to avoid size 0 if query length is 0.
	t_bidib_id_list_query per_query = bidib_get_connected_peripherals();
	GString *g_peripherals = g_string_sized_new(64 * (per_query.length + 1));
	g_string_assign(g_peripherals, "");
	append_start_of_obj(g_peripherals, false);
	append_field_start_of_list(g_peripherals, "peripherals");
	
	int added_peripherals = 0;
	for (size_t i = 0; i < per_query.length; i++) {
		t_bidib_peripheral_state_query per_state_query = bidib_get_peripheral_state(per_query.ids[i]);
		if (!per_state_query.available) {
			bidib_free_peripheral_state_query(per_state_query);
			syslog_server(LOG_WARNING, 
			              "Get pheripherals json - peripheral %s not available", 
			              per_query.ids[i]);
			continue;
		}
		if (added_peripherals++ > 0) {
			g_string_append_c(g_peripherals, ',');
		}
		
		append_start_of_obj(g_peripherals, true);
		append_field_str_value(g_peripherals, "id", per_query.ids[i], true);
		append_field_str_value(g_peripherals, "state-id", per_state_query.data.state_id, true);
		///TODO: Check if this works correctly; data.state_value is uint8, not unsigned int.
		append_field_uint_value(g_peripherals, "state-value", per_state_query.data.state_value, false);
		append_end_of_obj(g_peripherals, false);
		bidib_free_peripheral_state_query(per_state_query);
	}
	
	append_end_of_list(g_peripherals, false, added_peripherals > 0);
	append_end_of_obj(g_peripherals, false);
	bidib_free_id_list_query(per_query);
	return g_peripherals;
}

o_con_status handler_get_peripherals(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_peripherals = get_peripherals_json();
		send_some_gstring_and_free(res, HTTP_OK, g_peripherals);
		syslog_server(LOG_INFO, "Request: Get peripherals - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get peripherals");
	}
}

o_con_status handler_get_verification_option(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_GET) {
		onion_response_set_code(res, HTTP_OK);
		onion_response_printf(res, "{\n\"verification-enabled\": %s\n}", 
		                      verification_enabled ? "true" : "false");
		syslog_server(LOG_INFO, "Request: Get verification option - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get verification option");
	}
}

o_con_status handler_get_verification_url(void *_, onion_request *req, onion_response *res) {
    build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_GET) {
		const char *verif_url = get_verifier_url();
		onion_response_set_code(res, HTTP_OK);
		onion_response_printf(res, 
		                      "{\n\"verification-url\": \"%s\"\n}", 
		                      verif_url == NULL ? "null" : verif_url);
		syslog_server(LOG_INFO, "Request: Get verification url - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get verification url");
	}
}

static GString* get_granted_routes_json() {
	GString *g_granted_routes = g_string_sized_new(128);
	g_string_assign(g_granted_routes, "");
	
	// Old "normal" way without sized new
	//GString *g_granted_routes = g_string_new("");
	
	append_start_of_obj(g_granted_routes, false);
	append_field_start_of_list(g_granted_routes, "granted-routes");
	
	pthread_mutex_lock(&interlocker_mutex);
	GArray *route_ids = interlocking_table_get_all_route_ids_cpy();
	
	int routes_added = 0;
	if (route_ids != NULL) {
		for (size_t i = 0; i < route_ids->len; i++) {
			const char *route_id = g_array_index(route_ids, char *, i);
			if (route_id != NULL) {
				t_interlocking_route *route = get_route(route_id);
				if (route != NULL && route->train != NULL) {
					if (routes_added++ > 0) {
						g_string_append_c(g_granted_routes, ',');
					}
					append_start_of_obj(g_granted_routes, true);
					append_field_bare_value_from_str(g_granted_routes, "id", route->id, true);
					append_field_str_value(g_granted_routes, "train", route->train, false);
					append_end_of_obj(g_granted_routes, false);
				}
			}
		}
		// free the GArray but not the contained strings, as it was created by shallow copy.
		g_array_free(route_ids, true);
	}
	pthread_mutex_unlock(&interlocker_mutex);
	
	append_end_of_list(g_granted_routes, false, routes_added > 0);
	append_end_of_obj(g_granted_routes, false);
	return g_granted_routes;
}

o_con_status handler_get_granted_routes(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *g_granted_routes = get_granted_routes_json();
		send_some_gstring_and_free(res, HTTP_OK, g_granted_routes);
		syslog_server(LOG_INFO, "Request: Get granted routes - done");
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get granted routes");
	}
}

static GString* get_route_json(const char *route_id) {
	pthread_mutex_lock(&interlocker_mutex);
	const t_interlocking_route *route = get_route(route_id);
	if (route == NULL) {
		pthread_mutex_unlock(&interlocker_mutex);
		syslog_server(LOG_ERR, "Get route json - invalid route id (route not found)");
		return NULL;
	}
	GString *g_route = g_string_sized_new(1024);
	g_string_assign(g_route, "");
	
	append_start_of_obj(g_route, false);
	append_field_bare_value_from_str(g_route, "id", route->id, true);
	append_field_str_value(g_route, "source_signal", route->source, true);
	append_field_str_value(g_route, "destination_signal", route->destination, true);
	append_field_str_value(g_route, "orientation", route->orientation, true);
	append_field_float_value(g_route, "length", route->length, true);
	append_field_strlist_value_garray_strs(g_route, "path", route->path, true);
	append_field_strlist_value_garray_strs(g_route, "sections", route->sections, true);
	append_field_strlist_value_garray_strs(g_route, "signals", route->signals, true);
	
	GArray *g_point_ids = garray_points_to_garray_str_ids(route->points);
	append_field_strlist_value_garray_strs(g_route, "points", g_point_ids, true);
	free_g_strarray(g_point_ids);
	
	append_field_barelist_value_from_garray_strs(g_route, "conflicting_route_ids", 
	                                             route->conflicts, true);
	
	GArray *g_conflicts = get_granted_route_conflicts(route_id, false);
	append_field_barelist_value_from_garray_strs(g_route, "granted_conflicting_route_ids", 
	                                             g_conflicts, true);
	free_g_strarray(g_conflicts);
	
	append_field_bool_value(g_route, "clear", get_route_is_clear(route_id), true);
	append_field_str_value(g_route, "granted_to_train", 
	                       route->train == NULL ? "" : route->train, false);
	append_end_of_obj(g_route, false);
	pthread_mutex_unlock(&interlocker_mutex);
	return g_route;
}

o_con_status handler_get_route(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		if (route_id == NULL || strcmp(route_id, "") == 0 || get_route(route_id) == NULL) {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, "Request: Get route - invalid parameters");
			return OCS_PROCESSED;
		}
		
		syslog_server(LOG_INFO, "Request: Get route - route: %s - start", route_id);
		GString* g_route = get_route_json(route_id);
		if (g_route != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, g_route);
			syslog_server(LOG_INFO, "Request: Get route - route: %s - finished", route_id);
		} else {
			onion_response_set_code(res, HTTP_BAD_REQUEST);
			syslog_server(LOG_ERR, "Request: Get route - route: %s - invalid route", route_id);
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get route");
	}
}

// Returns debugging information related to the ForeC dynamic containers.
// Provides data values seen by the environment (dyn_containers_interface.c)
// and those set by the containers (dyn_containers.forec).
static GString *debug_info(void) {
	GString *info_str = g_string_new("");	

	const char info_template0[] = 
	    "Debug info: \n"
	    "* dyn_containers_reaction_counter: %lld \n"
	    "* dyn_containers_actuate_reaction_counter: %lld \n"
	    "\n";
	
	g_string_append_printf(
		info_str, info_template0, 
		dyn_containers_reaction_counter__global_0_0,
		dyn_containers_actuate_reaction_counter
	);

	const char info_template1[] = 
	    "dyn_containers_interface: (external value, internal value) \n"
	    "  running: %d \n"
	    "  terminate: %d, %d \n"
	    "  let_period_us: %d, %d \n"
	    "  \n";
	
	g_string_append_printf(
		info_str, info_template1, 
		dyn_containers_interface->running,
		dyn_containers_interface->terminate,     forec_intern_input__global_0_0.value.terminate,
		dyn_containers_interface->let_period_us, forec_intern_input__global_0_0.value.let_period_us
	);
	
	const char info_template2[] = 
	    "  train_engines_io[%d]: \n"
	    "    input_load: %d, %d \n"
	    "    input_unload: %d, %d \n"
	    "    input_filepath: \"%s\", \"%s\" \n"
	    "    output_in_use: %d, %d \n"
	    "    output_name: \"%s\", \"%s\" \n"
	    "  \n";
	
	t_forec_intern_input_train_engine__global_0_0
		    *forec_intern_input_train_engine[TRAIN_ENGINE_COUNT_MAX] = {
		&forec_intern_input_train_engine_0__global_0_0.value,
		&forec_intern_input_train_engine_1__global_0_0.value,
		&forec_intern_input_train_engine_2__global_0_0.value,
		&forec_intern_input_train_engine_3__global_0_0.value
	};
	t_forec_intern_output_train_engine__global_0_0
		    *forec_intern_output_train_engine[TRAIN_ENGINE_COUNT_MAX] = {
		&forec_intern_output_train_engine_0__global_0_0.value,
		&forec_intern_output_train_engine_1__global_0_0.value,
		&forec_intern_output_train_engine_2__global_0_0.value,
		&forec_intern_output_train_engine_3__global_0_0.value
	};
	for (int i = 0; i < TRAIN_ENGINE_COUNT_MAX; i++) {
		g_string_append_printf(
			info_str, info_template2,
			i,
			
			dyn_containers_interface->train_engines_io[i].input_load,
			forec_intern_input_train_engine[i]->load,
			
			dyn_containers_interface->train_engines_io[i].input_unload,
			forec_intern_input_train_engine[i]->unload,
			
			dyn_containers_interface->train_engines_io[i].input_filepath,
			forec_intern_input_train_engine[i]->filepath,
			
			dyn_containers_interface->train_engines_io[i].output_in_use,
			forec_intern_output_train_engine[i]->in_use,
			
			dyn_containers_interface->train_engines_io[i].output_name,
			forec_intern_output_train_engine[i]->name
		);
	}
	
	const char info_template3[] = 
	    "  train_engine_instances_io[%d] \n"
	    "    input_grab: %d, %d \n"
	    "    input_release: %d, %d \n"
	    "    input_train_engine_type: %d, %d \n"
	    "    input_requested_speed: %d, %d \n"
	    "    input_requested_forwards: %d, %d \n"
	    "    output_in_use: %d, %d \n"
	    "    output_train_engine_type: %d, %d \n"
	    "    output_nominal_speed: %d, %d \n"
	    "    output_nominal_forwards: %d, %d \n"
	    "  \n";
	
	t_forec_intern_input_train_engine_instance__global_0_0 
			*forec_intern_input_train_engine_instance[TRAIN_ENGINE_INSTANCE_COUNT_MAX] = {
		&forec_intern_input_train_engine_instance_0__global_0_0.value,
		&forec_intern_input_train_engine_instance_1__global_0_0.value,
		&forec_intern_input_train_engine_instance_2__global_0_0.value,
		&forec_intern_input_train_engine_instance_3__global_0_0.value,
		&forec_intern_input_train_engine_instance_4__global_0_0.value
	};
	t_forec_intern_output_train_engine_instance__global_0_0
			*forec_intern_output_train_engine_instance[TRAIN_ENGINE_INSTANCE_COUNT_MAX] = {
		&forec_intern_output_train_engine_instance_0__global_0_0.value,
		&forec_intern_output_train_engine_instance_1__global_0_0.value,
		&forec_intern_output_train_engine_instance_2__global_0_0.value,
		&forec_intern_output_train_engine_instance_3__global_0_0.value,
		&forec_intern_output_train_engine_instance_4__global_0_0.value,
	};
	for (int i = 0; i < TRAIN_ENGINE_INSTANCE_COUNT_MAX; i++) {
		g_string_append_printf(
			info_str, info_template3,
			i,
			
			dyn_containers_interface->train_engine_instances_io[i].input_grab,
			forec_intern_input_train_engine_instance[i]->grab,
			
			dyn_containers_interface->train_engine_instances_io[i].input_release,
			forec_intern_input_train_engine_instance[i]->release,
			
			dyn_containers_interface->train_engine_instances_io[i].input_train_engine_type,
			forec_intern_input_train_engine_instance[i]->train_engine_type,
			
			dyn_containers_interface->train_engine_instances_io[i].input_requested_speed,
			forec_intern_input_train_engine_instance[i]->requested_speed,
			
			dyn_containers_interface->train_engine_instances_io[i].input_requested_forwards,
			forec_intern_input_train_engine_instance[i]->requested_forwards,
			
			dyn_containers_interface->train_engine_instances_io[i].output_in_use,
			forec_intern_output_train_engine_instance[i]->in_use,
			
			dyn_containers_interface->train_engine_instances_io[i].output_train_engine_type,
			forec_intern_output_train_engine_instance[i]->train_engine_type,
			
			dyn_containers_interface->train_engine_instances_io[i].output_nominal_speed,
			forec_intern_output_train_engine_instance[i]->nominal_speed,
			
			dyn_containers_interface->train_engine_instances_io[i].output_nominal_forwards,
			forec_intern_output_train_engine_instance[i]->nominal_forwards
		);
	}
	
	return info_str;
}

o_con_status handler_get_debug_info(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *debug_info_str = debug_info();
		if (debug_info_str != NULL && debug_info_str->str != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, debug_info_str);
			syslog_server(LOG_NOTICE, "Request: Get debug info - done");
		} else {
			onion_response_set_code(res, HTTP_INTERNAL_ERROR);
			syslog_server(LOG_ERR, "Request: Get debug info - internal error");
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get debug info");
	}
}

// Returns extra debugging information related to the ForeC thread scheduler
// in dyn_containers.forec.
static GString *debug_info_extra(void) {
	GString *info_str = g_string_new("");	

	const char info_template[] = 
	    "Debug info extra: \n"
	    "* mainParReactionCounter: %d \n"
	    "* mainParCore1.reactionCounter: %d \n"
	    "* mainParCore2.reactionCounter: %d \n"
	    "\n";

	g_string_append_printf(
		info_str, info_template, 
		mainParReactionCounter,
		mainParCore1.reactionCounter,
		mainParCore2.reactionCounter
	);
	
	return info_str;
}

o_con_status handler_get_debug_info_extra(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_GET)) {
		GString *debug_info_extra_str = debug_info_extra();
		if (debug_info_extra_str != NULL && debug_info_extra_str->str != NULL) {
			send_some_gstring_and_free(res, HTTP_OK, debug_info_extra_str);
			syslog_server(LOG_NOTICE, "Request: Get debug info extra");
		} else {
			onion_response_set_code(res, HTTP_INTERNAL_ERROR);
			syslog_server(LOG_ERR, "Request: Get debug info extra - internal error");
		}
		return OCS_PROCESSED;
	} else {
		return handle_req_run_or_method_fail(res, running, "Get debug info extra");
	}
}
