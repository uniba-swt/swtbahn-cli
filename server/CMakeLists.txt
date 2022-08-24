cmake_minimum_required(VERSION 3.6)
project(swtbahn-cli)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wno-gnu-folding-constant")
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(FindPkgConfig)

pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

pkg_check_modules(GNUTLS gnutls REQUIRED)
include_directories(${GNUTLS_INCLUDE_DIRS})
link_directories(${GNUTLS_LIBRARY_DIRS})

pkg_check_modules(CMOCKA cmocka REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIRS})
link_directories(${CMOCKA_LIBRARY_DIRS})

IF (APPLE)
	set(LIBRARY_DIR /usr/local/Cellar)

	# Manually add the include and library directories for libgcrypt
	include_directories(${LIBRARY_DIR}/libgcrypt/1.10.1/include)
	link_directories(${LIBRARY_DIR}/libgcrypt/1.10.1/lib)

	# Manually add the include and library directories for yaml
	include_directories(${LIBRARY_DIR}/libyaml/0.2.5/include)
	link_directories(${LIBRARY_DIR}/libyaml/0.2.5/lib)

	# Manually add the include and library directories for libev
	set(LINK_EV ev)
	include_directories(${LIBRARY_DIR}/libev/4.33/include)
	link_directories(${LIBRARY_DIR}/libev/4.33/lib)
	
	# Manually add the include and library directories for onion
	set(LIBRARY_DIR /Users/eugeneyip/Documents/SWTbahn/Software)
	include_directories(${LIBRARY_DIR}/onion/src)
	link_directories(${LIBRARY_DIR}/onion/bin/src/onion)
ELSE ()
	set(LIBRARY_DIR /home/pi/SWTbahn)
ENDIF (APPLE)

file(GLOB SRCFILES "src/*.c" "src/parsers/*.c")

set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/engines;${CMAKE_CURRENT_BINARY_DIR}/interlockers")


# - - - - - - - - - - - - -
# ADJUSTMENT SECTION BEGIN
# - - - - - - - - - - - - -

set(LIBONION_DIR ${LIBRARY_DIR}/onion)
add_library(onion_static STATIC IMPORTED)
set_target_properties(onion_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBONION_DIR}/src)
set_target_properties(onion_static PROPERTIES IMPORTED_LOCATION
	${LIBONION_DIR}/bin/src/onion/libonion_static${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(onion SHARED IMPORTED)
set_target_properties(onion PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBONION_DIR}/src)
set_target_properties(onion PROPERTIES IMPORTED_LOCATION
	${LIBONION_DIR}/bin/src/onion/libonion${CMAKE_SHARED_LIBRARY_SUFFIX})

set(LIBBIDIB_DIR ${LIBRARY_DIR}/libbidib)
add_library(bidib_static STATIC IMPORTED)
set_target_properties(bidib_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBBIDIB_DIR}/include)
set_target_properties(bidib_static PROPERTIES IMPORTED_LOCATION
	${LIBBIDIB_DIR}/bin/libbidib_static${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(bidib SHARED IMPORTED)
set_target_properties(bidib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBBIDIB_DIR}/include)
set_target_properties(bidib PROPERTIES IMPORTED_LOCATION
	${LIBBIDIB_DIR}/bin/libbidib${CMAKE_SHARED_LIBRARY_SUFFIX})

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


# Unit tests

enable_testing()

set(UNIT_TESTS server_parser_tests server_bahn_util_tests)

foreach(UNIT_TEST ${UNIT_TESTS})
	add_executable(${UNIT_TEST} test src/bahn_data_util.c src/parsers/ test/unit/${UNIT_TEST}.c)
	target_link_libraries(${UNIT_TEST} glib-2.0 cmocka pthread yaml onion bidib ${FOREC_MAIN})
	add_test(${UNIT_TEST} ${UNIT_TEST})
endforeach()
