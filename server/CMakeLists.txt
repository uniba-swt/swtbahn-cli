cmake_minimum_required(VERSION 3.6)
project(swtbahn-cli)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS -Wall)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

file(GLOB SRCFILES "src/*.c")

# - - - - - - - - - - - - -
# ADJUSTMENT SECTION BEGIN
# - - - - - - - - - - - - -

set(LIBONION_DIR /home/pi/SWTbahn/onion)
add_library(onion_static STATIC IMPORTED)
set_target_properties(onion_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBONION_DIR}/src)
set_target_properties(onion_static PROPERTIES IMPORTED_LOCATION
	${LIBONION_DIR}/bin/src/onion/libonion_static.a)

set(LIBBIDIB_DIR /home/pi/SWTbahn/libbidib)
add_library(bidib_static STATIC IMPORTED)
set_target_properties(bidib_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBBIDIB_DIR}/include)
set_target_properties(bidib_static PROPERTIES IMPORTED_LOCATION
	${LIBBIDIB_DIR}/bin/libbidib_static.a)

set(FOREC_MAIN dyn_containers)
set(FOREC_SRC ${CMAKE_SOURCE_DIR}/src)
add_custom_target(
	forec
	COMMAND forecc -mforec_dyn_containers ${FOREC_SRC}/${FOREC_MAIN}.forec ${FOREC_SRC}/${FOREC_MAIN}.foreh
	BYPRODUCTS ${FOREC_MAIN}.c debug.txt information.xml ${FOREC_MAIN}.dot 
)
add_custom_target(
	dot
	COMMAND dot -T pdf ${FOREC_MAIN}.dot -o ${FOREC_MAIN}.pdf
	DEPENDS forec
	BYPRODUCTS ${FOREC_MAIN}.pdf
)

# - - - - - - - - - - - - -
# ADJUSTMENT SECTION END
# - - - - - - - - - - - - -

add_library(${FOREC_MAIN}_static STATIC ${SRCFILES} ${FOREC_MAIN}.c)
add_dependencies(${FOREC_MAIN}_static forec dot)
target_include_directories(${FOREC_MAIN}_static PRIVATE src)
target_link_libraries(${FOREC_MAIN}_static bidib)

add_executable(swtbahn-server src ${SRCFILES})
target_link_libraries(swtbahn-server onion_static pam gnutls gcrypt pthread
	glib-2.0 yaml bidib_static)
