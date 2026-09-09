###############################################################################
# Spherical azimuthal map projection plugin for the SeisComP GUI
#
# Builds  libmapazimuthal.so  - a runtime-loadable plugin that adds the
# "Orthographic", "Stereographic" and "AzimuthalEquidistant" projections to
# libseiscomp_gui and therefore to every SeisComP GUI application.
#
# SeisComP ships no CMake package config, so this is a standalone project
# that locates the SDK through SEISCOMP_ROOT.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DSEISCOMP_ROOT=/opt/seiscomp
#   cmake --build build
#   cmake --install build            # -> $SEISCOMP_ROOT/share/plugins
###############################################################################

cmake_minimum_required(VERSION 3.10)
project(mapazimuthal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# --- Locate the SeisComP installation ----------------------------------------
if(NOT SEISCOMP_ROOT)
	if(DEFINED ENV{SEISCOMP_ROOT})
		set(SEISCOMP_ROOT $ENV{SEISCOMP_ROOT})
	else()
		set(SEISCOMP_ROOT "/opt/seiscomp")
	endif()
endif()
message(STATUS "SEISCOMP_ROOT = ${SEISCOMP_ROOT}")

find_path(SEISCOMP_INCLUDE_DIR
	NAMES seiscomp/gui/map/projection.h
	HINTS "${SEISCOMP_ROOT}/include"
	REQUIRED
)

find_library(SEISCOMP_CORE_LIB   NAMES seiscomp_core   HINTS "${SEISCOMP_ROOT}/lib" REQUIRED)
find_library(SEISCOMP_CLIENT_LIB NAMES seiscomp_client HINTS "${SEISCOMP_ROOT}/lib" REQUIRED)
find_library(SEISCOMP_GUI_LIB
	NAMES seiscomp_gui seiscomp_qt
	HINTS "${SEISCOMP_ROOT}/lib"
	REQUIRED
)
message(STATUS "SeisComP GUI library: ${SEISCOMP_GUI_LIB}")

find_package(Qt5 REQUIRED COMPONENTS Core Gui)

# --- Plugin target -----------------------------------------------------------
add_library(mapazimuthal MODULE
	azimuthal.cpp
	azimuthal.h
	projections.cpp
)

set_target_properties(mapazimuthal PROPERTIES
	PREFIX "lib"
	OUTPUT_NAME "mapazimuthal"
)

target_include_directories(mapazimuthal PRIVATE
	"${SEISCOMP_INCLUDE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(mapazimuthal PRIVATE
	${SEISCOMP_GUI_LIB}
	${SEISCOMP_CLIENT_LIB}
	${SEISCOMP_CORE_LIB}
	Qt5::Core
	Qt5::Gui
)

target_compile_options(mapazimuthal PRIVATE -Wall -Wextra)

# --- Regression test (optional) --------------------------------------------
option(BUILD_TESTS "Build the offscreen regression test" OFF)
if(BUILD_TESTS)
	enable_testing()
	add_executable(regression tests/regression.cpp)
	target_include_directories(regression PRIVATE "${SEISCOMP_INCLUDE_DIR}")
	target_link_libraries(regression PRIVATE
		${SEISCOMP_GUI_LIB}
		${SEISCOMP_CLIENT_LIB}
		${SEISCOMP_CORE_LIB}
		Qt5::Core
		Qt5::Gui
		${CMAKE_DL_LIBS}
	)
	add_test(NAME regression COMMAND regression $<TARGET_FILE:mapazimuthal>)
	set_tests_properties(regression PROPERTIES
		ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LD_LIBRARY_PATH=${SEISCOMP_ROOT}/lib"
	)
endif()

# --- Install ---------------------------------------------------------------
install(TARGETS mapazimuthal
	LIBRARY DESTINATION "${SEISCOMP_ROOT}/share/plugins"
)
install(FILES etc/descriptions/mapprojections.xml
	DESTINATION "${SEISCOMP_ROOT}/etc/descriptions"
)
