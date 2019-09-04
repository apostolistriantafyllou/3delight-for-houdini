#  Specify the minimum required version of CMake to build the project.
cmake_minimum_required( VERSION 3.6 )
project( 3Delight_for_Houdini )

# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
find_package( Houdini REQUIRED )

# Find TBB in HFS
IF(APPLE)
	string(REGEX REPLACE "/Resources$" "" HRES "$ENV{HFS}")
	link_directories(${HRES}/Libraries)
	MESSAGE(STATUS "Dynamically linking to Houdini's TBB")
ENDIF()

# Set the 3Delight path
if(NOT DEFINED DELIGHT)
	set(DELIGHT $ENV{DELIGHT})
ENDIF()

# Add a library
set( library_name 3Delight_for_Houdini )
add_library( ${library_name} SHARED
	ROP_3Delight.cpp
	VOP_ExternalOSL.cpp
	camera.cpp
	curvemesh.cpp
	exporter.cpp
	instance.cpp
	jsonServer.cpp
	light.cpp
	mplay.cpp
	polygonmesh.cpp
	pointmesh.cpp
	null.cpp
	osl_utilities.cpp
	plugin.cpp
	scene.cpp
	shader_library.cpp
	vop.cpp
	ui/settings.cpp
	ui/aov.cpp
	ui/select_layers_dialog.cpp )

add_subdirectory( osl osl )

# 3Delight includes
target_include_directories( ${library_name} PRIVATE "${DELIGHT}/include" )

# Link against the Houdini libraries, and add required include directories and
# compile definitions. Special handling needed for tbb on macOS.
IF(APPLE)
	target_link_libraries( ${library_name} Houdini tbb)
ELSE()
    target_link_libraries( ${library_name} Houdini)
ENDIF()

# This will get the destination directory for the installation.
if(NOT DEFINED INSTALL_DIR)
	houdini_get_default_install_dir( INSTALL_DIR )
ENDIF()

install(
	FILES ui/ROP_3Delight.svg
	DESTINATION ${INSTALL_DIR}/config/Icons )

install(
	FILES ui/ROP_3Delight.svg
	RENAME ROP_3DelightCloud.svg
	DESTINATION ${INSTALL_DIR}/config/Icons )

install(
	FILES ui/select_layers_ui.ui
	DESTINATION ${INSTALL_DIR}/config/Applications )

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} "INSTDIR" "${INSTALL_DIR}/dso" )
