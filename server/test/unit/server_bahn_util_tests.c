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
	bahn_data_util_init_cached_track_state();
}

static void test_teardown(void) {
	bahn_data_util_free_cached_track_state();
}

static void no_parser_errors(void **state) {
	assert_int_equal(bahn_data_util_initialise_config(config_directory), 1);
}

static void type_segment(void **state) {
	bool segment_type = is_type_segment("seg1");
	assert_true(segment_type);
	
	segment_type = is_type_segment("signal1");
	assert_false(segment_type);
}

static void type_signal(void **state) {
	bool signal_type = is_type_signal("seg1");
	assert_false(signal_type);
	
	signal_type = is_type_signal("signal1");
	assert_true(signal_type);
}

static void type_none(void **state) {
	bool signal_type = is_type_signal("?");
	assert_false(signal_type);
	
	signal_type = is_type_signal("?");
	assert_false(signal_type);
}

static void main_segments(void **state) {
	char *segments[1024];
	int count = config_get_array_string_value("block", "block14", "main_segments", segments);
	assert_non_null(segments);
	assert_int_equal(1, count);
	assert_string_equal("seg63", segments[0]);

	count = config_get_array_string_value("block", "block19", "main_segments", segments);
	assert_non_null(segments);
	assert_int_equal(2, count);
	assert_string_equal("seg82a", segments[0]);
	assert_string_equal("seg82b", segments[1]);
}

static void overlaps(void **state) {
	char *overlaps[1024];
	const int count = config_get_array_string_value("block", "block1", "overlaps", overlaps);
	
	assert_non_null(overlaps);
	assert_int_equal(2, count);
	assert_string_equal("seg1", overlaps[0]);
	assert_string_equal("seg3", overlaps[1]);
}


int main(int argc, char **argv) {
	openlog("swtbahn", 0, LOG_LOCAL0);
	syslog(LOG_INFO, "server_bahn_util_tests: %s", "Bahn util tests started");

	const struct CMUnitTest tests[] = {
			cmocka_unit_test(no_parser_errors),
			cmocka_unit_test(type_segment),
			cmocka_unit_test(type_signal),
			cmocka_unit_test(type_none),
			cmocka_unit_test(main_segments),
			cmocka_unit_test(overlaps)
	};
	
	test_setup();
	int ret = cmocka_run_group_tests(tests, NULL, NULL);
	test_teardown();

	syslog(LOG_INFO, "server_bahn_util_tests: %s", "Bahn util tests stopped");
	closelog();
	return ret;
}
