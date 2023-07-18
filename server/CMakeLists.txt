cmake_minimum_required(VERSION 3.6)
project(swtbahn-cli)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall -Wno-gnu-folding-constant")
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

pkg_check_modules(GNUTLS gnutls REQUIRED)
include_directories(${GNUTLS_INCLUDE_DIRS})
link_directories(${GNUTLS_LIBRARY_DIRS})

pkg_check_modules(CMOCKA cmocka REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIRS})
link_directories(${CMOCKA_LIBRARY_DIRS})

# Include BiDiB library that have been installed
pkg_check_modules(BIDIB bidib REQUIRED)
include_directories(${BIDIB_INCLUDE_DIRS})
link_directories(${BIDIB_LIBRARY_DIRS})

pkg_check_modules(ONION onion REQUIRED)
include_directories(${ONION_INCLUDE_DIRS})
link_directories(${ONION_LIBRARY_DIRS})

IF (APPLE)
	pkg_check_modules(LIBGCRYPT libgcrypt REQUIRED)
	include_directories(${LIBGCRYPT_INCLUDE_DIRS})
	link_directories(${LIBGCRYPT_LIBRARY_DIRS})

	pkg_check_modules(YAML yaml-0.1 REQUIRED)
	include_directories(${YAML_INCLUDE_DIRS})
	link_directories(${YAML_LIBRARY_DIRS})

	# Manually add the include and library directories for libev
	set(LINK_EV ev)
	set(LIB_EV_DIR /usr/local/Cellar/libev/4.33)
	include_directories(${LIB_EV_DIR}/include)
	link_directories(${LIB_EV_DIR}/lib)
ENDIF (APPLE)

file(GLOB SRCFILES "src/*.c" "src/parsers/*.c" "src/check_route_sectional/*.c")

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
target_include_directories(${FOREC_MAIN} PRIVATE src)
target_link_libraries(${FOREC_MAIN} bidib onion glib-2.0 yaml)

add_library(${FOREC_MAIN}_static STATIC ${SRCFILES} ${FOREC_MAIN}.c)
target_include_directories(${FOREC_MAIN}_static PRIVATE src)
target_link_libraries(${FOREC_MAIN}_static bidib_static onion_static glib-2.0 yaml)

add_executable(swtbahn-server src ${SRCFILES})
target_link_libraries(swtbahn-server onion pam gnutls gcrypt pthread
	glib-2.0 yaml bidib ${LINK_EV} ${FOREC_MAIN} ${CMAKE_DL_LIBS})
	
target_compile_options(swtbahn-server PRIVATE -fno-omit-frame-pointer -fsanitize=address)
target_link_options(swtbahn-server PRIVATE -fno-omit-frame-pointer -fsanitize=address)

# Unit tests

enable_testing()

set(UNIT_TESTS server_parser_tests server_bahn_util_tests)

foreach(UNIT_TEST ${UNIT_TESTS})
	add_executable(${UNIT_TEST} test src/bahn_data_util.c src/parsers/ test/unit/${UNIT_TEST}.c)
	target_link_libraries(${UNIT_TEST} glib-2.0 cmocka pthread yaml onion bidib ${FOREC_MAIN})
	add_test(${UNIT_TEST} ${UNIT_TEST})
endforeach()
