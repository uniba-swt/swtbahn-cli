/*
 *
 * Copyright (C) 2022 University of Bamberg, Software Technologies Research Group
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
 * - Eugene Yip <https://github.com/eyip002>
 *
 */
 
#include <syslog.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <bidib/bidib.h>

#include "../../src/bahn_data_util.h"

static const char *config_directory = "../../configurations/swtbahn-full/";

static void test_setup(void) {
	// Probably not necessary as no test calls track_state_get_value -> investigate
	bahn_data_util_init_cached_track_state();
}

static void test_teardown(void) {
	// Probably not necessary as no test calls track_state_get_value -> investigate
	bahn_data_util_free_cached_track_state();
}

static void no_parser_errors(void **state) {
	assert_int_equal(bahn_data_util_initialise_config(config_directory), 1);
}

static void route_information(void **state) {
	char *route_ids[1024];
	int route_count = interlocking_table_get_routes("signal22a", "signal37", route_ids);
	assert_int_equal(route_count, 1);
	assert_string_equal(route_ids[0], "0");
	
	char *id = config_get_scalar_string_value("route", route_ids[0], "id");
	assert_string_equal(id, "0");

	char *source = config_get_scalar_string_value("route", route_ids[0], "source");
	assert_string_equal(source, "signal22a");

	char *destination = config_get_scalar_string_value("route", route_ids[0], "destination");
	assert_string_equal(destination, "signal37");
	
	char *path_ids[1024];
	int path_count = config_get_array_string_value("route", route_ids[0], "path", path_ids);
	assert_int_equal(path_count, 6);
	assert_string_equal(path_ids[0], "seg33");
	assert_string_equal(path_ids[1], "seg34");
	assert_string_equal(path_ids[2], "seg35");
	assert_string_equal(path_ids[3], "seg67");
	assert_string_equal(path_ids[4], "seg68");
	assert_string_equal(path_ids[5], "seg69");
	
	char *sections_ids[1024];
	int sections_count = config_get_array_string_value("route", route_ids[0], "sections", sections_ids);
	assert_int_equal(sections_count, 1);
	assert_string_equal(sections_ids[0], "block15");

	char *point_ids[1024];
	int point_count = config_get_array_string_value("route", route_ids[0], "route_points", point_ids);
	assert_int_equal(point_count, 3);
	assert_string_equal(point_ids[0], "point11");
	assert_string_equal(point_ids[1], "point12");
	assert_string_equal(point_ids[2], "point13");
	
	char *conflict_ids[1024];
	int conflict_count = config_get_array_string_value("route", route_ids[0], "conflicts", conflict_ids);
	assert_int_equal(conflict_count, 51);
	assert_string_equal(conflict_ids[0], "1");
	assert_string_equal(conflict_ids[conflict_count - 1], "151");

	char *signal_ids[1024];
	int signal_count = config_get_array_string_value("route", route_ids[0], "route_signals", signal_ids);
	assert_int_equal(signal_count, 2);
	assert_string_equal(signal_ids[0], "signal22a");
	assert_string_equal(signal_ids[1], "signal37");
	
	float length = config_get_scalar_float_value("route", route_ids[0], "length");
	assert_float_equal(length, 197.30, 0.0);
	
	char *orientation = config_get_scalar_string_value("route", route_ids[0], "orientation");
	assert_string_equal(orientation, "anticlockwise");
}

static void point_position(void **state) {
	char *route_id_0 = "0"; 
	char *position = config_get_point_position(route_id_0, "point11");
	assert_string_equal(position, "normal");
	
	char *route_id_1 = "1";
	position = config_get_point_position(route_id_1, "point11");
	assert_string_equal(position, "reverse");
}

static void extra_information(void **state) {
	char *segment_id = config_get_scalar_string_value("segment", "seg2", "id");
	assert_string_equal(segment_id, "seg2");
	
	float segment_length = config_get_scalar_float_value("segment", "seg2", "length");
	assert_float_equal(segment_length, 187.1, 0.0);

	char *point_id = config_get_scalar_string_value("point", "point6", "id");
	assert_string_equal(point_id, "point6");
	
	char *point_segment = config_get_scalar_string_value ("point", "point6", "segment");
	assert_string_equal(point_segment, "seg18");
	
	char *peripheral_id = config_get_scalar_string_value("peripheral", "sync2", "id");
	assert_string_equal(peripheral_id, "sync2");

	char *peripheral_type_id = config_get_scalar_string_value("peripheraltype", "onebit", "id");
	assert_string_equal(peripheral_type_id, "onebit");

	char *peripheral_type = config_get_scalar_string_value("peripheral", "sync2", "type");
	assert_string_equal(peripheral_type, "onebit");
	
	char *crossing_id = config_get_scalar_string_value("crossing", "crossing1", "id");
	assert_string_equal(crossing_id, "crossing1");

	char *crossing_segment = config_get_scalar_string_value ("crossing", "crossing1", "segment");
	assert_string_equal(crossing_segment, "seg20");

	char *reverser_id = config_get_scalar_string_value("reverser", "reverser", "id");
	assert_string_equal(reverser_id, "reverser");
}

static void signal_type(void **state) {
	char *id = config_get_scalar_string_value("signal", "signal14", "id");
	assert_string_equal(id, "signal14");
	
	char *signal_type = config_get_scalar_string_value("signal", "signal14", "type");
	assert_string_equal(signal_type, "entry");

	signal_type = config_get_scalar_string_value("signal", "signal22a", "type");
	assert_string_equal(signal_type, "exit");
	
	signal_type = config_get_scalar_string_value("signal", "signal5", "type");
	assert_string_equal(signal_type, "block");

	signal_type = config_get_scalar_string_value("signal", "signal39", "type");
	assert_string_equal(signal_type, "shunting");

	signal_type = config_get_scalar_string_value("signal", "signal40", "type");
	assert_string_equal(signal_type, "halt");

	signal_type = config_get_scalar_string_value("signal", "signal6", "type");
	assert_string_equal(signal_type, "distant");
	
	signal_type = config_get_scalar_string_value("signal", "platformlight4b", "type");
	assert_string_equal(signal_type, "platformlight");

	char *signal_type_id = config_get_scalar_string_value("signaltype", "distant", "id");
	assert_string_equal(signal_type_id, "distant");
}

static void train_information(void **state) {
	char *id = config_get_scalar_string_value("train", "cargo_db", "id");
	assert_string_equal(id, "cargo_db");
	
	char *type = config_get_scalar_string_value("train", "cargo_db", "type");
	assert_string_equal(type, "cargo");
	
	float weight = config_get_scalar_float_value("train", "cargo_db", "weight");
	assert_float_equal(weight, 77.0, 0.0);
	
	float length = config_get_scalar_float_value("train", "cargo_db", "length");
	assert_float_equal(length, 13.0, 0.0);
}

static void block_information(void ** state) {
	char *id = config_get_scalar_string_value("block", "block1", "id");
	assert_string_equal(id, "block1");

	char *segments[1024];
	int segments_count = config_get_array_string_value("block" , "block1", "main_segments", segments);
	assert_non_null(segments);
	assert_int_equal(1, segments_count);
	assert_string_equal(segments[0], "seg2");

	char *overlap_ids[1024];
	int overlap_count = config_get_array_string_value("block", "block1", "overlaps", overlap_ids);
	assert_int_equal(overlap_count, 2);
	assert_string_equal(overlap_ids[0], "seg1");
	assert_string_equal(overlap_ids[1], "seg3");

	bool is_reversed = config_get_scalar_bool_value("block", "block1", "is_reversed");
	assert_false(is_reversed);
	
	char *direction = config_get_scalar_string_value("block" , "block1", "direction");
	assert_string_equal(direction, "bidirectional");

	float length = config_get_scalar_float_value("block", "block1", "length");
	assert_float_equal(length, 220.20, 0.0);
	
	int limit = config_get_scalar_float_value("block", "block1", "limit");
	assert_float_equal(limit, 300.0, 0.0);
	
	char *train_types[1024];
	int train_types_count = config_get_array_string_value("block", "block1", "train_types", train_types);
	assert_int_equal(train_types_count, 2);
	assert_string_equal(train_types[0], "cargo");
	assert_string_equal(train_types[1], "passenger");
	
	char *signal_ids[1024];
	int signals_count = config_get_array_string_value("block", "block1", "block_signals", signal_ids);
	assert_int_equal(signals_count, 2);
	assert_string_equal(signal_ids[0], "signal1");
	assert_string_equal(signal_ids[1], "signal4a");
}

static void reverser_information(void ** state) {
	char *reverser_id = config_get_scalar_string_value("reverser", "reverser", "id");
	assert_string_equal(reverser_id, "reverser");

	char *reverser_board = config_get_scalar_string_value("reverser", "reverser", "board");
	assert_string_equal(reverser_board, "master");
	
	char *reverser_block = config_get_scalar_string_value("reverser", "reverser", "block");
	assert_string_equal(reverser_block, "block14");
}

static void signal_composition(void **state) {
	char *id = config_get_scalar_string_value("composition", "signal4", "id");
	assert_string_equal(id, "signal4");

	char *entry = config_get_scalar_string_value("composition" , "signal4", "entry");
	assert_string_equal(entry, "signal4a");
	
	char *distant = config_get_scalar_string_value("composition" , "signal4", "distant");
	assert_string_equal(distant, "signal4b");
	
	char *exit = config_get_scalar_string_value("composition", "signal22", "exit");
	assert_string_equal(exit, "signal22a");
	
	char *block = config_get_scalar_string_value("composition" , "signal7", "block");
	assert_string_equal(block, "signal7a");
}

static void invalid_ids(void **state) {
	char *route_id = config_get_scalar_string_value("route", "?", "id");
	assert_string_equal(route_id, "");

	char *segment_id = config_get_scalar_string_value("segment", "?", "id");
	assert_string_equal(segment_id, "");

	char *point_id = config_get_scalar_string_value("point", "?", "id");
	assert_string_equal(point_id, "");

	char *signal_id = config_get_scalar_string_value("signal", "?", "id");
	assert_string_equal(signal_id, "");
	
	char *peripheral_id = config_get_scalar_string_value("peripheral", "?", "id");
	assert_string_equal(peripheral_id, "");

	char *crossing_id = config_get_scalar_string_value("crossing", "?", "id");
	assert_string_equal(crossing_id, "");
	
	char *train_id = config_get_scalar_string_value("train", "?", "id");
	assert_string_equal(train_id, "");

	char *block_id = config_get_scalar_string_value("block", "?", "id");
	assert_string_equal(block_id, "");

	char *composition_id = config_get_scalar_string_value("composition", "?", "id");
	assert_string_equal(composition_id, "");
}

int main(int argc, char **argv) {
	openlog("swtbahn", 0, LOG_LOCAL0);
	syslog(LOG_INFO, "server_parser_tests: %s", "Config-parser tests started");

	const struct CMUnitTest tests[] = {
			cmocka_unit_test(no_parser_errors),
			cmocka_unit_test(route_information),
			cmocka_unit_test(point_position),
			cmocka_unit_test(extra_information),
			cmocka_unit_test(signal_type),
			cmocka_unit_test(train_information),
			cmocka_unit_test(block_information),
			cmocka_unit_test(reverser_information),
			cmocka_unit_test(signal_composition),
			cmocka_unit_test(invalid_ids)
	};
	
	test_setup();
	int ret = cmocka_run_group_tests(tests, NULL, NULL);
	test_teardown();
	
	syslog(LOG_INFO, "server_parser_tests: %s", "Config-parser tests stopped");
	closelog();
	return ret;
}
