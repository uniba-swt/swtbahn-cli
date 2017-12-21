cmake_minimum_required(VERSION 3.6)
project(swtbahn-cli)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS -Wall)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})


# ADJUSTMENT SECTION BEGIN


set(LIBONION_DIR /home/nicolas/git/onion)
add_library(onion_static STATIC IMPORTED)
set_target_properties(onion_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBONION_DIR}/src)
set_target_properties(onion_static PROPERTIES IMPORTED_LOCATION
	${LIBONION_DIR}/bin/src/onion/libonion_static.a)

set(LIBBIDIB_DIR /home/nicolas/git/libbidib)
add_library(bidib_static STATIC IMPORTED)
set_target_properties(bidib_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${LIBBIDIB_DIR}/include)
set_target_properties(bidib_static PROPERTIES IMPORTED_LOCATION
	${LIBBIDIB_DIR}/bin/libbidib_static.a)


# ADJUSTMENT SECTION END


file(GLOB SRCFILES
	"src/*.c"
	)

add_executable(swtbahn-server src ${SRCFILES})
target_link_libraries(swtbahn-server onion_static pam gnutls gcrypt pthread
	glib-2.0 yaml bidib_static)
