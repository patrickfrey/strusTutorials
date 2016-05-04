cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

set( strus_INCLUDE_DIRS "/usr/local/include/strus" )
set( strus_LIBRARY_DIRS "/usr/local/lib/strus" )
set( LIB_INSTALL_DIR "/usr/local/lib" )

set( CMAKE_CXX_FLAGS "-std=c++98  -Wall -pedantic -g -Wfatal-errors -fvisibility=hidden -fPIC -O0" )

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
include_directories(
	"${strus_INCLUDE_DIRS}"
)

link_directories( 
	"${strus_LIBRARY_DIRS}"
)

# ------------------------------------
# LIBRARY
# ------------------------------------
add_library( modstrus_trace_example  MODULE  modstrus_trace_example.cpp )
set_target_properties( modstrus_trace_example PROPERTIES PREFIX "")
target_link_libraries( modstrus_trace_example strus_module )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS modstrus_trace_example
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus/modules
	   RUNTIME DESTINATION bin )



