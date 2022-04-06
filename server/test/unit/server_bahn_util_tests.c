#include <syslog.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <bidib.h>

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

int main(int argc, char **argv) {
	openlog("swtbahn", 0, LOG_LOCAL0);
	syslog(LOG_INFO, "server_bahn_util_tests: %s", "Bahn util tests started");

	const struct CMUnitTest tests[] = {
			cmocka_unit_test(no_parser_errors),
			cmocka_unit_test(type_segment),
			cmocka_unit_test(type_signal),
			cmocka_unit_test(type_none)
	};
	
	test_setup();
	int ret = cmocka_run_group_tests(tests, NULL, NULL);
	test_teardown();

	syslog(LOG_INFO, "server_bahn_util_tests: %s", "Bahn util tests stopped");
	closelog();
	return ret;
}
