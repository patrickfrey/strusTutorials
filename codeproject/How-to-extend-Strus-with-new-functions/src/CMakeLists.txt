cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

set( strus_INCLUDE_DIRS "/usr/local/include/strus" )
set( strus_LIBRARY_DIRS "/usr/local/lib/strus" )
set( LIB_INSTALL_DIR "/usr/local/lib" )

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
add_library( modstrus_storage_example  MODULE  modstrus_storage_example.cpp positionWindow.cpp window_joinop.cpp minwin_weighting.cpp minwin_summarizer.cpp)
set_target_properties( modstrus_storage_example PROPERTIES PREFIX "")
target_link_libraries( modstrus_storage_example strus_module )


# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS modstrus_storage_example
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus/modules
	   RUNTIME DESTINATION bin )



