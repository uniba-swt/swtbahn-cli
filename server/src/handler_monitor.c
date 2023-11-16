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


GArray* garray_points_to_garray_str_ids(GArray *points) {
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

void free_g_strarray(GArray *g_strarray) {
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

void sprintf_garray_char(GString *output, GArray *garray) {
	if (garray->len == 0) {
		g_string_append_printf(output, "none");
		return;
	}
	for (size_t i = 0; i < garray->len; i++) {
		g_string_append_printf(output, "%s%s", 
		                       g_array_index(garray, char *, i),
		                       i != (garray->len - 1) ? ", " : "");
	}
}

void sprintf_garray_interlocking_point(GString *output, GArray *garray) {
	if (garray->len == 0) {
		g_string_append_printf(output, "none");
		return;
	}
	for (size_t i = 0; i < garray->len; i++) {
		g_string_append_printf(output, "%s%s", 
		                       g_array_index(garray, t_interlocking_point, i).id,
		                       i != (garray->len - 1) ? ", " : "");
	}
}

GString *get_trains_json() {
	t_bidib_id_list_query query = bidib_get_trains();
	GString *g_trains = g_string_sized_new(64 * (query.length + 1));
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
	
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_trains_json", 72 * (query.length + 1), g_trains->len);
	bidib_free_id_list_query(query);
	return g_trains;
}

onion_connection_status handler_get_trains(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get available trains");
		GString *g_trains = get_trains_json();
		onion_response_printf(res, "%s", g_trains->str);
		syslog_server(LOG_INFO, "Request: Get available trains - finished");
		g_string_free(g_trains, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get available trains - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_train_state_json(const char *data_train) {
	if (data_train == NULL) {
		///TODO: Discuss what to return (NULL or empty string? Or "empty" JSON?)
		return g_string_new("");
	}
	
	t_bidib_train_state_query train_state_query = bidib_get_train_state(data_train);
	if (!train_state_query.known) {
		bidib_free_train_state_query(train_state_query);
		///TODO: Discuss what to return (NULL or empty str? Or "empty" JSON, or containing err msg?)
		return g_string_new("");
	}
	
	GString *g_train_state = g_string_sized_new(384);
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
		append_end_of_list(g_train_state, false, added_blocks > 0);
	}
	
	bidib_free_train_position_query(train_position_query);
	append_end_of_obj(g_train_state, false);
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_train_state_json", 384, g_train_state->len);
	return g_train_state;
}

onion_connection_status handler_get_train_state(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train state - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} 
		
		syslog_server(LOG_INFO, "Request: Get train state - train: %s", data_train);
		GString *ret_string = get_train_state_json(data_train);
		if (ret_string == NULL || ret_string->len == 0) {
			syslog_server(LOG_ERR, 
			              "Request: Get train state - train: %s - invalid train", 
			              data_train);
			return OCS_NOT_IMPLEMENTED;
		} else {
			onion_response_printf(res, "%s", ret_string->str);
			syslog_server(LOG_INFO, "Request: Get train state - train: %s - finished", data_train);
			g_string_free(ret_string, true);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get train state - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

///TODO: Discuss, change in behaviour: train with no peripherals now does not lead to an error.
GString *get_train_peripherals_json(const char *data_train) {
	if (data_train == NULL) {
		///TODO: Discuss what to return (NULL or empty string? Or "empty" JSON?)
		return g_string_new("");
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
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_train_peripherals_json", 64 * (query.length + 1), g_train_peripherals->len);
	
	bidib_free_id_list_query(query);
	return g_train_peripherals;
}

onion_connection_status handler_get_train_peripherals(void *_, onion_request *req,
                                                      onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train peripherals - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		
		syslog_server(LOG_INFO, "Request: Get train peripherals - train: %s", data_train);
		GString *g_train_peripherals = get_train_peripherals_json(data_train);
		onion_response_printf(res, "%s", g_train_peripherals->str);
		syslog_server(LOG_INFO, 
		              "Request: Get train peripherals - train: %s - finished",
		              data_train);
		g_string_free(g_train_peripherals, true);
		return OCS_PROCESSED;
		
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get train peripherals - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_track_outputs_json() {
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
			if (track_outputs_added++ > 0) {
				g_string_append_c(g_track_outputs, ',');
			}
			append_start_of_obj(g_track_outputs, true);
			append_field_str_value(g_track_outputs, "id", query.ids[i], true);
			append_field_str_value(g_track_outputs, "state", state_string, false);
			append_end_of_obj(g_track_outputs, false);
		}
	}
	append_end_of_list(g_track_outputs, false, track_outputs_added > 0);
	append_end_of_obj(g_track_outputs, false);
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_track_outputs_json", 48 * (query.length + 1), g_track_outputs->len);
	bidib_free_id_list_query(query);
	return g_track_outputs;
}

onion_connection_status handler_get_track_outputs(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get track outputs");
		GString *g_track_outputs = get_track_outputs_json();
		onion_response_printf(res, "%s", g_track_outputs->str);
		syslog_server(LOG_INFO, "Request: Get track outputs - finished");
		g_string_free(g_track_outputs, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get track outputs - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_accessory_json(bool point_accessories) {
	t_bidib_id_list_query query;
	if (point_accessories) {
		query = bidib_get_connected_points();
	} else {
		query = bidib_get_connected_signals();
	}
	GString *g_accs = g_string_sized_new(80 * (query.length + 1));
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
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_accessory_json", 64 * (query.length + 1), g_accs->len);
	bidib_free_id_list_query(query);
	return g_accs;
}

onion_connection_status handler_get_points(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get points");
		GString *g_points = get_accessory_json(true);
		onion_response_printf(res, "%s", g_points->str);
		syslog_server(LOG_INFO, "Request: Get points - finished");
		g_string_free(g_points, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get points - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_signals(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get signals");
		GString *g_signals = get_accessory_json(false);
		onion_response_printf(res, "%s", g_signals->str);
		syslog_server(LOG_INFO, "Request: Get signals - finished");
		g_string_free(g_signals, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get signals - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_accessory_aspects_json(const char *data_id, bool is_point) {
	if (data_id == NULL) {
		///TODO: Discuss ret val
		return g_string_new("");
	}
	t_bidib_id_list_query query;
	if (is_point) {
		query = bidib_get_point_aspects(data_id);
	} else {
		query = bidib_get_signal_aspects(data_id);
	}
	
	if (query.length <= 0 || query.ids == NULL) {
		///TODO: Discuss ret val
		return g_string_new("");
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
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_accessory_aspects_json", query.length * 16, g_aspects->len);
	bidib_free_id_list_query(query);
	return g_aspects;
}

onion_connection_status handler_get_point_aspects(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		if (data_point == NULL) {
			syslog_server(LOG_ERR, "Request: Get point aspects - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		
		syslog_server(LOG_INFO, "Request: Get point aspects - point: %s", data_point);
		GString *g_aspects = get_accessory_aspects_json(data_point, true);
		if (g_aspects == NULL || g_aspects->len <= 0) {
			syslog_server(LOG_ERR, 
			              "Request: Get point aspects - point: %s - invalid point", 
			              data_point);
			return OCS_NOT_IMPLEMENTED;
		} else {
			onion_response_printf(res, "%s", g_aspects->str);
			syslog_server(LOG_INFO, "Request: Get point aspects - point: %s - finished", data_point);
			g_string_free(g_aspects, true);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get point aspects - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_signal_aspects(void *_, onion_request *req,
                                                   onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_signal = onion_request_get_post(req, "signal");
		if (data_signal == NULL) {
			syslog_server(LOG_ERR, "Request: Get signal aspects - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		
		syslog_server(LOG_INFO, "Request: Get signal aspects - signal: %s", data_signal);
		GString *g_aspects = get_accessory_aspects_json(data_signal, false);
		if (g_aspects == NULL || g_aspects->len <= 0) {
			syslog_server(LOG_ERR, 
			              "Request: Get signal aspects - signal: %s - invalid signal", 
			              data_signal);
			return OCS_NOT_IMPLEMENTED;
		} else {
			onion_response_printf(res, "%s", g_aspects->str);
			syslog_server(LOG_INFO, 
			              "Request: Get signal aspects - signal: %s - finished", 
			              data_signal);
			g_string_free(g_aspects, true);
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get signal aspects - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_segments_json() {
	t_bidib_id_list_query seg_query = bidib_get_connected_segments();
	
	GString *g_segments = g_string_sized_new(48 * (seg_query.length + 1));
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
		
		append_end_of_list(g_segments, false, seg_state_query.data.dcc_address_cnt > 0);
		append_end_of_obj(g_segments, false);
		
		bidib_free_segment_state_query(seg_state_query);
		
	}
	append_end_of_list(g_segments, false, added_segments > 0);
	append_end_of_obj(g_segments, false);
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_segments_json", 48 * (seg_query.length + 1), g_segments->len);
	bidib_free_id_list_query(seg_query);
	return g_segments;
}

onion_connection_status handler_get_segments(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get segments");
		GString *g_segments = get_segments_json();
		onion_response_printf(res, "%s", g_segments->str);
		syslog_server(LOG_INFO, "Request: Get segments - finished");
		g_string_free(g_segments, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR,  "Request: Get segments - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_reversers_json() {
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
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_reversers_json", 48 * (rev_query.length + 1), g_reversers->len);
	bidib_free_id_list_query(rev_query);
	return g_reversers;
}

onion_connection_status handler_get_reversers(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get reversers");
		if (!reversers_state_update()) {
			syslog_server(LOG_ERR, "Request: Get reversers - unable to request state update");
			return OCS_NOT_IMPLEMENTED;
		}
		GString *g_reversers = get_reversers_json();
		onion_response_printf(res, "%s", g_reversers->str);
		syslog_server(LOG_INFO, "Request: Get reversers - finished");
		g_string_free(g_reversers, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get reversers - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString *get_peripherals_json() {
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
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_peripherals_json", 64 * (per_query.length + 1), g_peripherals->len);
	bidib_free_id_list_query(per_query);
	return g_peripherals;
}

onion_connection_status handler_get_peripherals(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get peripherals");
		GString *g_peripherals = get_peripherals_json();
		onion_response_printf(res, "%s", g_peripherals->str);
		syslog_server(LOG_INFO, "Request: Get peripherals - finished");
		g_string_free(g_peripherals, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get peripherals - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_verification_option(void *_, onion_request *req,
                                                        onion_response *res) {
	build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_GET) {
		onion_response_printf(res, "{\n\"verification-enabled\": %s\n}", 
		                      verification_enabled ? "true" : "false");
		syslog_server(LOG_INFO, "Request: Get verification option - done");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get verification option - wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_verification_url(void *_, onion_request *req,
                                                     onion_response *res) {
    build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_GET) {
		const char *verif_url = get_verifier_url();
		onion_response_printf(res, 
		                      "{\n\"verification-url\": \"%s\"\n}", 
		                      verif_url == NULL ? "null" : verif_url);
		
		syslog_server(LOG_INFO, "Request: Get verification url - done");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get verification url - wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString* get_granted_routes_json() {
	///TODO: test - sized new could be nice to avoid reallocs, but now how do initialize the string?
	//              maybe just with g_string_assign?
	GString *g_granted_routes = g_string_sized_new(64);
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
		g_array_free(route_ids, true);
	}
	pthread_mutex_unlock(&interlocker_mutex);
	
	append_end_of_list(g_granted_routes, false, routes_added > 0);
	append_end_of_obj(g_granted_routes, false);
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_granted_routes_json", 256, g_granted_routes->len);
	
	return g_granted_routes;
}

onion_connection_status handler_get_granted_routes(void *_, onion_request *req,
                                                   onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		syslog_server(LOG_INFO, "Request: Get granted routes");
		GString *g_granted_routes = get_granted_routes_json();
		onion_response_printf(res, "%s", g_granted_routes->str);
		syslog_server(LOG_INFO, "Request: Get granted routes - finished");
		g_string_free(g_granted_routes, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get granted routes - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

GString* get_route_json(const char *route_id) {
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
	
	append_field_strlist_value_garray_strs(g_route, "granted_conflicting_route_ids", 
	                                       get_granted_route_conflicts(route_id), true);
	
	append_field_bool_value(g_route, "clear", get_route_is_clear(route_id), true);
	append_field_str_value(g_route, "granted_to_train", 
	                       route->train == NULL ? "" : route->train, false);
	append_end_of_obj(g_route, false);
	syslog_server(LOG_NOTICE, "%s - size estimate: %zu, size actual: %zu", "get_route_json", 1024, g_route->len);
	pthread_mutex_unlock(&interlocker_mutex);
	return g_route;
}

onion_connection_status handler_get_route(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		if (route_id == NULL || strcmp(route_id, "") == 0 || get_route(route_id) == NULL) {
			syslog_server(LOG_ERR, "Request: Get route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		}
		
		syslog_server(LOG_INFO, "Request: Get route - route: %s", route_id);
		GString* g_route = get_route_json(route_id);
		onion_response_printf(res, "%s", g_route->str);
		syslog_server(LOG_INFO, "Request: Get route - route: %s - finished", route_id);
		g_string_free(g_route, true);
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get route - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

// Returns debugging information related to the ForeC dynamic containers.
// Provides data values seen by the environment (dyn_containers_interface.c)
// and those set by the containers (dyn_containers.forec).
GString *debug_info(void) {
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

onion_connection_status handler_get_debug_info(void *_, onion_request *req, onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *debug_info_str = debug_info();
		char response[strlen(debug_info_str->str) + 1];
		strcpy(response, debug_info_str->str);
		g_string_free(debug_info_str, true);
		
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get debug info");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get debug info - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

// Returns extra debugging information related to the ForeC thread scheduler
// in dyn_containers.forec.
GString *debug_info_extra(void) {
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

onion_connection_status handler_get_debug_info_extra(void *_, onion_request *req,
                                                     onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *debug_info_extra_str = debug_info_extra();
		char response[strlen(debug_info_extra_str->str) + 1];
		strcpy(response, debug_info_extra_str->str);
		g_string_free(debug_info_extra_str, true);
		
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get debug info extra");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, 
		              "Request: Get debug info extra - system not running or wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
