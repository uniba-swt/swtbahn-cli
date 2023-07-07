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

#include <onion/onion.h>
#include <bidib/bidib.h>
#include <pthread.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>

#include "server.h"
#include "handler_driver.h"
#include "handler_controller.h"
#include "bahn_data_util.h"
#include "interlocking.h"
#include "param_verification.h"



onion_connection_status handler_get_trains(void *_, onion_request *req,
                                           onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *trains = g_string_new("");
		t_bidib_id_list_query query = bidib_get_trains();
		for (size_t i = 0; i < query.length; i++) {
			g_string_append_printf(trains, "%s%s - grabbed: %s",
			                       i != 0 ? "\n" : "", query.ids[i],
			                       train_grabbed(query.ids[i]) ? "yes" : "no");
		}
		bidib_free_id_list_query(query);
		char response[trains->len + 1];
		strcpy(response, trains->str);
		g_string_free(trains, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get available trains");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get available trains - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_train_state(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train state - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			t_bidib_train_state_query train_state_query =
				bidib_get_train_state(data_train);
			t_bidib_train_position_query train_position_query = 
				bidib_get_train_position(data_train);
			if (train_state_query.known) {
				GString *seg_string = g_string_new("no");
				GString *block_string = g_string_new("no");
				if (train_position_query.length > 0) {
					g_string_printf(seg_string, "%s", train_position_query.segments[0]);
					for (size_t i = 1; i < train_position_query.length; i++) {
						g_string_append_printf(seg_string, ", %s", train_position_query.segments[i]);
					}

					for (size_t i = 0; i < train_position_query.length; i++) {
						const char *block_id =
								config_get_block_id_of_segment(train_position_query.segments[i]);
						if (block_id != NULL) {
							g_string_printf(block_string, "%s", block_id);
							break;
						}
					}
				}
				bidib_free_train_position_query(train_position_query);
			
				GString *ret_string = g_string_new("");
				g_string_append_printf(ret_string, "grabbed: %s - on segment: %s - on block: %s"
				                       " - orientation: %s"
				                       " - speed step: %d - detected speed: %d km/h - direction: %s",
				                       train_grabbed(data_train) ? "yes" : "no",
				                       seg_string->str,
				                       block_string->str,
				                       (train_state_query.data.orientation ==
				                       BIDIB_TRAIN_ORIENTATION_LEFT) ?
				                       "left" : "right",
				                       train_state_query.data.set_speed_step,
				                       train_state_query.data.detected_kmh_speed,
				                       train_state_query.data.set_is_forwards
				                       ? "forwards" : "backwards");
				bidib_free_train_state_query(train_state_query);
				char response[ret_string->len + 1];
				strcpy(response, ret_string->str);
				g_string_free(seg_string, true);
				g_string_free(ret_string, true);
				onion_response_printf(res, "%s", response);
				syslog_server(LOG_NOTICE, "Request: Get train state");
				return OCS_PROCESSED;
			} else {
				bidib_free_train_position_query(train_position_query);
				bidib_free_train_state_query(train_state_query);
				syslog_server(LOG_ERR, "Request: Get train state - invalid train");
				return OCS_NOT_IMPLEMENTED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get train state - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_train_peripherals(void *_, onion_request *req,
                                                      onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_train = onion_request_get_post(req, "train");
		if (data_train == NULL) {
			syslog_server(LOG_ERR, "Request: Get train peripherals - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			t_bidib_id_list_query query =
				bidib_get_train_peripherals(data_train);
			if (query.length > 0) {
				GString *train_peripherals = g_string_new("");
				for (size_t i = 0; i < query.length; i++) {
					t_bidib_train_peripheral_state_query per_state =
						bidib_get_train_peripheral_state(data_train, query.ids[i]);
					g_string_append_printf(train_peripherals, "%s%s - state: %s",
					                       i != 0 ? "\n" : "", query.ids[i],
					                       per_state.state == 1 ? "on" : "off");
				}
				bidib_free_id_list_query(query);
				char response[train_peripherals->len + 1];
				strcpy(response, train_peripherals->str);
				g_string_free(train_peripherals, true);
				onion_response_printf(res, "%s", response);
				syslog_server(LOG_NOTICE, "Request: Get train peripherals");
				return OCS_PROCESSED;
			} else {
				bidib_free_id_list_query(query);
				syslog_server(LOG_ERR, "Request: Get train train peripherals - invalid "
				              "train");
				return OCS_NOT_IMPLEMENTED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get train peripherals - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_track_outputs(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *track_outputs = g_string_new("");
		t_bidib_id_list_query query = bidib_get_track_outputs();
		for (size_t i = 0; i < query.length; i++) {
			t_bidib_track_output_state_query track_output_state =
				bidib_get_track_output_state(query.ids[i]);
			if (track_output_state.known) {
				char *state_string;
				switch (track_output_state.cs_state) {
					case 0x00:
						state_string = "off";
						break;
					case 0x01:
						state_string = "stop";
						break;
					case 0x02:
						state_string = "soft stop";
						break;
					case 0x03:
						state_string = "go";
						break;
					case 0x04:
						state_string = "go + ignore watchdog";
						break;
					case 0x08:
						state_string = "prog";
						break;
					case 0x09:
						state_string = "prog busy";
						break;
					case 0x0D:
						state_string = "busy";
						break;
					case 0xFF:
						state_string = "query";
						break;
				}
				g_string_append_printf(track_outputs, "%s%s - state: %s",
				                       i != 0 ? "\n" : "", query.ids[i],
				                       state_string);
			}
		}
		bidib_free_id_list_query(query);
		char response[track_outputs->len + 1];
		strcpy(response, track_outputs->str);
		g_string_free(track_outputs, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get track outputs");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get track outputs - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_points(void *_, onion_request *req,
                                           onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *points = g_string_new("");
		t_bidib_id_list_query query = bidib_get_connected_points();
		for (size_t i = 0; i < query.length; i++) {
			t_bidib_unified_accessory_state_query point_state =
				bidib_get_point_state(query.ids[i]);
			
			GString *execution_state = g_string_new("");
			if (point_state.type == BIDIB_ACCESSORY_BOARD) {
				g_string_printf(execution_state, "(target state%s reached)", 
				                point_state.board_accessory_state.execution_state ? 
				                " not" : "");
			}
			char execution_state_str[execution_state->len + 1];
			strcpy(execution_state_str, execution_state->str);
			g_string_free(execution_state, true);
			
			g_string_append_printf(points, "%s%s - state: %s %s",
			                       i != 0 ? "\n" : "", query.ids[i],
			                       point_state.type == BIDIB_ACCESSORY_BOARD ?
			                       point_state.board_accessory_state.state_id :
			                       point_state.dcc_accessory_state.state_id,
			                       execution_state_str);
			bidib_free_unified_accessory_state_query(point_state);
		}
		bidib_free_id_list_query(query);
		char response[points->len + 1];
		strcpy(response, points->str);
		g_string_free(points, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get points");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get points - system not running or wrong "
		              "request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_signals(void *_, onion_request *req,
                                            onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *signals = g_string_new("");
		t_bidib_id_list_query query = bidib_get_connected_signals();
		for (size_t i = 0; i < query.length; i++) {
			t_bidib_unified_accessory_state_query signal_state =
				bidib_get_signal_state(query.ids[i]);
			g_string_append_printf(signals, "%s%s - state: %s",
			                       i != 0 ? "\n" : "", query.ids[i],
			                       signal_state.type == BIDIB_ACCESSORY_BOARD ?
			                       signal_state.board_accessory_state.state_id :
			                       signal_state.dcc_accessory_state.state_id);
			bidib_free_unified_accessory_state_query(signal_state);
		}
		bidib_free_id_list_query(query);
		char response[signals->len + 1];
		strcpy(response, signals->str);
		g_string_free(signals, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get signals");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get signals - system not running or wrong "
		              "request type");
		return OCS_NOT_IMPLEMENTED;
	}
}


onion_connection_status handler_get_point_aspects(void *_, onion_request *req,
                                                  onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_point = onion_request_get_post(req, "point");
		if (data_point == NULL) {
			syslog_server(LOG_ERR, "Request: Get point aspects - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			t_bidib_id_list_query query = bidib_get_point_aspects(data_point);
			if (query.length > 0) {
				GString *aspects = g_string_new("");
				for (size_t i = 0; i < query.length; i++) {
					g_string_append_printf(aspects, "%s%s", i != 0 ? ", " : "",
					                       query.ids[i]);
				}
				bidib_free_id_list_query(query);
				char response[aspects->len + 1];
				strcpy(response, aspects->str);
				g_string_free(aspects, true);
				onion_response_printf(res, "%s", response);
				syslog_server(LOG_NOTICE, "Request: Get point aspects");
				return OCS_PROCESSED;
			} else {
				bidib_free_id_list_query(query);
				syslog_server(LOG_ERR, "Request: Get point aspects - invalid point");
				return OCS_NOT_IMPLEMENTED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get point aspects - system not running or "
		              "wrong request type");
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
		} else {
			t_bidib_id_list_query query = bidib_get_signal_aspects(data_signal);
			if (query.length > 0) {
				GString *aspects = g_string_new("");
				for (size_t i = 0; i < query.length; i++) {
					g_string_append_printf(aspects, "%s%s", i != 0 ? ", " : "",
					                       query.ids[i]);
				}
				bidib_free_id_list_query(query);
				char response[aspects->len + 1];
				strcpy(response, aspects->str);
				g_string_free(aspects, true);
				onion_response_printf(res, "%s", response);
				syslog_server(LOG_NOTICE, "Request: Get signal aspects");
				return OCS_PROCESSED;
			} else {
				bidib_free_id_list_query(query);
				syslog_server(LOG_ERR, "Request: Get signal aspects - invalid signal");
				return OCS_NOT_IMPLEMENTED;
			}
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get signal aspects - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_segments(void *_, onion_request *req,
                                             onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *segments = g_string_new("");
		t_bidib_id_list_query seg_query = bidib_get_connected_segments();
		for (size_t i = 0; i < seg_query.length; i++) {
			t_bidib_segment_state_query seg_state_query =
				bidib_get_segment_state(seg_query.ids[i]);
			g_string_append_printf(segments, "%s%s - occupied: %s",
			                       i != 0 ? "\n" : "", seg_query.ids[i],
			                       seg_state_query.data.occupied ? "yes" : "no");
			if (seg_state_query.data.dcc_address_cnt > 0) {
				g_string_append_printf(segments, " trains: ");
				t_bidib_id_query id_query;
				for (size_t j = 0; j < seg_state_query.data.dcc_address_cnt; j++) {
					id_query = bidib_get_train_id(seg_state_query.data.dcc_addresses[j]);
					if (id_query.known) {
						g_string_append_printf(segments, "%s%s",
						                       j != 0 ? ", " : "", id_query.id);
					} else {
						g_string_append_printf(segments, "%s%s",
						                       j != 0 ? ", " : "", "unknown");
					}
					bidib_free_id_query(id_query);
				}
			}
			bidib_free_segment_state_query(seg_state_query);
		}
		bidib_free_id_list_query(seg_query);
		char response[segments->len + 1];
		strcpy(response, segments->str);
		g_string_free(segments, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get segments");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get segments - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_reversers(void *_, onion_request *req,
                                              onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		if (!reversers_state_update()) {
			syslog_server(LOG_ERR, "Request: Get reversers - unable to request state update");
			return OCS_NOT_IMPLEMENTED;
		} else {
			GString *reversers = g_string_new("");
			t_bidib_id_list_query rev_query = bidib_get_connected_reversers();
			for (size_t i = 0; i < rev_query.length; i++) {
				const char *reverser_id = rev_query.ids[i];
				t_bidib_reverser_state_query rev_state_query =
						bidib_get_reverser_state(reverser_id);
				if (!rev_state_query.available) {
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
						
				g_string_append_printf(reversers, "%s%s - state: %s",
									   i != 0 ? "\n" : "",
									   reverser_id, state_value_str);
				bidib_free_reverser_state_query(rev_state_query);
			}
			bidib_free_id_list_query(rev_query);
			char response[reversers->len + 1];
			strcpy(response, reversers->str);
			g_string_free(reversers, true);
			onion_response_printf(res, "%s", response);
			syslog_server(LOG_NOTICE, "Request: Get reversers");
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get reversers - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_peripherals(void *_, onion_request *req,
                                                onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		GString *peripherals = g_string_new("");
		t_bidib_id_list_query per_query = bidib_get_connected_peripherals();
		for (size_t i = 0; i < per_query.length; i++) {
			t_bidib_peripheral_state_query per_state_query =
				bidib_get_peripheral_state(per_query.ids[i]);
			g_string_append_printf(peripherals, "%s%s - %s: %d",
			                       i != 0 ? "\n" : "", per_query.ids[i],
			                       per_state_query.data.state_id,
			                       per_state_query.data.state_value);
			bidib_free_peripheral_state_query(per_state_query);
		}
		bidib_free_id_list_query(per_query);
		char response[peripherals->len + 1];
		strcpy(response, peripherals->str);
		g_string_free(peripherals, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get peripherals");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get peripherals - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_verification_option(void *_, onion_request *req,
                                                        onion_response *res) {
	build_response_header(res);
	if ((onion_request_get_flags(req) & OR_METHODS) == OR_GET) {
		onion_response_printf(res, "verification-enabled: %s", 
		                      verification_enabled ? "true" : "false");
		syslog_server(LOG_NOTICE, "Request: Get verification option");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get verification option - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}

onion_connection_status handler_get_granted_routes(void *_, onion_request *req,
                                                   onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		bool needNewLine = false;
		GString *granted_routes = g_string_new("");
		GArray *route_ids = interlocking_table_get_all_route_ids();
		for (size_t i = 0; i < route_ids->len; i++) {
			const char *route_id = g_array_index(route_ids, char *, i);
			t_interlocking_route *route = get_route(route_id);
			if (route->train != NULL) {
				g_string_append_printf(granted_routes, "%sroute id: %s train: %s", 
				                       needNewLine ? "\n" : "",
				                       route->id, route->train);
				needNewLine = true;
			}
		}
		if (strcmp(granted_routes->str, "") == 0) {
			g_string_append_printf(granted_routes, "No granted routes");
		}
		g_array_free(route_ids, true);
		char response[granted_routes->len + 1];
		strcpy(response, granted_routes->str);
		g_string_free(granted_routes, true);
		onion_response_printf(res, "%s", response);
		syslog_server(LOG_NOTICE, "Request: Get granted routes");
		return OCS_PROCESSED;
	} else {
		syslog_server(LOG_ERR, "Request: Get granted routes - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
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

onion_connection_status handler_get_route(void *_, onion_request *req,
                                          onion_response *res) {
	build_response_header(res);
	if (running && ((onion_request_get_flags(req) & OR_METHODS) == OR_POST)) {
		const char *data_route_id = onion_request_get_post(req, "route-id");
		const char *route_id = params_check_route_id(data_route_id);
		if (route_id == NULL || strcmp(route_id, "") == 0 || get_route(route_id) == NULL) {
			syslog_server(LOG_ERR, "Request: Get route - invalid parameters");
			return OCS_NOT_IMPLEMENTED;
		} else {
			GString *route_str = g_string_new("");
			t_interlocking_route *route = get_route(route_id);
			g_string_append_printf(route_str, "route id: %s\n", route->id);
			g_string_append_printf(route_str, "  source signal: %s\n", route->source);
			g_string_append_printf(route_str, "  destination signal: %s\n", route->destination);
			g_string_append_printf(route_str, "  orientation: %s\n", route->orientation);
			g_string_append_printf(route_str, "  length: %f\n", route->length);
			g_string_append_printf(route_str, "  path: ");
			sprintf_garray_char(route_str, route->path);
			g_string_append_printf(route_str, "\n  sections: ");
			sprintf_garray_char(route_str, route->sections);
			g_string_append_printf(route_str, "\n  points: ");
			sprintf_garray_interlocking_point(route_str, route->points);
			g_string_append_printf(route_str, "\n  signals: ");
			sprintf_garray_char(route_str, route->signals);
			g_string_append_printf(route_str, "\n  conflicting route ids: ");
			sprintf_garray_char(route_str, route->conflicts);
			
			g_string_append_printf(route_str, "\nstatus:");
			g_string_append_printf(route_str, "\n  granted conflicting route ids: ");
			GArray *granted_route_conflicts = get_granted_route_conflicts(route_id);
			sprintf_garray_char(route_str, granted_route_conflicts);
			g_array_free(granted_route_conflicts, true);
			
			pthread_mutex_lock(&interlocker_mutex);
			g_string_append_printf(route_str, "\n  route clear: %s", 
			                       get_route_is_clear(route_id) ? "yes": "no");
			pthread_mutex_unlock(&interlocker_mutex);
			
			g_string_append_printf(route_str, "\n  granted train: %s", 
			                       route->train == NULL ? "none" : route->train);
			
			char response[route_str->len + 1];
			strcpy(response, route_str->str);
			g_string_free(route_str, true);
			onion_response_printf(res, "%s", response);
			syslog_server(LOG_NOTICE, "Request: Get route");
			return OCS_PROCESSED;
		}
	} else {
		syslog_server(LOG_ERR, "Request: Get route - system not running or "
		              "wrong request type");
		return OCS_NOT_IMPLEMENTED;
	}
}
