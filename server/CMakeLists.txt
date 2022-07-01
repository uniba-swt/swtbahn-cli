cmake_minimum_required(VERSION 3.6)
project(swtbahn-cli)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS -Wall)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(libonion onion REQUIRED)
include_directories(${LIBONION_INCLUDE_DIRS})

# Include bibib libraries. Relies on Bidib having been installed prior.
pkg_check_modules(libbidib bidib REQUIRED)
pkg_check_modules(libbidib_static bidib_static REQUIRED)
include_directories(${LIBBIDIB_INCLUDE_DIRS})

file(GLOB SRCFILES "src/*.c" "src/parsers/*.c")

set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/engines;${CMAKE_CURRENT_BINARY_DIR}/interlockers")


# - - - - - - - - - - - - -
# ADJUSTMENT SECTION BEGIN
# - - - - - - - - - - - - -

set(FOREC_MAIN dyn_containers)
set(FOREC_SRC ${CMAKE_SOURCE_DIR}/src)
add_custom_command(
	OUTPUT ${FOREC_MAIN}.c debug.txt information.xml ${FOREC_MAIN}.dot ${FOREC_MAIN}.pdf
	COMMAND forecc -mforec_dyn_containers ${FOREC_SRC}/${FOREC_MAIN}.forec ${FOREC_SRC}/${FOREC_MAIN}.foreh
	COMMAND dot -T pdf ${FOREC_MAIN}.dot -o ${FOREC_MAIN}.pdf
	DEPENDS ${FOREC_SRC}/${FOREC_MAIN}.forec ${FOREC_SRC}/${FOREC_MAIN}.foreh
)

add_custom_target(
	engines_dir
	ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/engines"
)

add_custom_target(
	interlockers_dir
	ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/interlockers"
)


# - - - - - - - - - - - - -
# ADJUSTMENT SECTION END
# - - - - - - - - - - - - -

add_library(${FOREC_MAIN} SHARED ${SRCFILES} ${FOREC_MAIN}.c)
target_include_directories(${FOREC_MAIN} PRIVATE SYSTEM src)
target_link_libraries(${FOREC_MAIN} bidib onion)

add_library(${FOREC_MAIN}_static STATIC ${SRCFILES} ${FOREC_MAIN}.c)
target_include_directories(${FOREC_MAIN}_static PRIVATE SYSTEM src)
target_link_libraries(${FOREC_MAIN}_static bidib_static onion_static)

add_executable(swtbahn-server src ${SRCFILES})
target_link_libraries(swtbahn-server onion pam gnutls gcrypt pthread
	glib-2.0 yaml bidib ${FOREC_MAIN} ${CMAKE_DL_LIBS})


# Unit tests

enable_testing()

set(UNIT_TESTS server_parser_tests server_bahn_util_tests)

foreach(UNIT_TEST ${UNIT_TESTS})
	add_executable(${UNIT_TEST} test src/bahn_data_util.c src/parsers/ test/unit/${UNIT_TEST}.c)
	target_link_libraries(${UNIT_TEST} glib-2.0 cmocka pthread yaml onion bidib ${FOREC_MAIN})
	add_test(${UNIT_TEST} ${UNIT_TEST})
endforeach()
