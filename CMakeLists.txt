
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
	OBJ_IncandescenceLight.cpp
	ROP_3Delight.cpp
	VOP_3DelightMaterialBuilder.cpp
	VOP_ExternalOSL.cpp
	attributes_callbacks.cpp
	camera.cpp
	cop_utilities.cpp
	curvemesh.cpp
	dl_system.cpp
	exporter.cpp
	geometry.cpp
	idisplay_port.cpp
	incandescence_light.cpp
	instance.cpp
	light.cpp
	polygonmesh.cpp
	pointmesh.cpp
	primitive.cpp
	null.cpp
	object_attributes.cpp
	osl_utilities.cpp
	plugin.cpp
	safe_interest.cpp
	scene.cpp
	shader_library.cpp
	time_sampler.cpp
	vdb.cpp
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
	target_link_libraries( ${library_name} Houdini tbb hboost_thread)
ELSE()
    target_link_libraries( ${library_name} Houdini)
ENDIF()

# This will get the destination directory for the installation.
# WARNING: running houdini_get_default_install_dir will run "hyton" and
# this can result in mayhem, at least on macOS (crashes on my system).
# It also takes a Houdini license AND tries to load the 3Delight plugin
# we are trying to compile ... So you can set the INSTALL_DIR manually,
# like this:
#   cmake -DINSTALL_DIR=/Users/aghiles/Library/Preferences/houdini/18.0/ ...
# or even easier on Linux:
#   cnake -DINSTALL_DIR=$HIH ..
#
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
	FILES ui/vdb_no_outline.png ui/shelf_ROP_3Delight.png ui/shelf_ROP_3Delight_cloud.png ui/shelf_ROP_render_3Delight.png ui/shelf_ROP_select_3Delight.png ui/VOP_3Delight-dlHairAndFur.png ui/VOP_3Delight-dlPrincipled.png ui/VOP_3Delight-dlMetal.png ui/VOP_3Delight-dlSkin.png ui/VOP_3Delight-dlGlass.png ui/VOP_3Delight-dlSubstance.png ui/VOP_3Delight-dlColorToFloat.png ui/VOP_3Delight-dlFloatToColor.png ui/VOP_3Delight-dlRamp.png ui/VOP_3Delight-dlToon.png ui/VOP_3Delight-dlThin.png ui/VOP_3Delight-vdbVolume.png ui/OBJ_3Delight-dlIncandescenceLight.png
	DESTINATION ${INSTALL_DIR}/config/Icons )

install(
	FILES ui/select_layers_ui.ui
	DESTINATION ${INSTALL_DIR}/config/Applications )

install(
	FILES ui/geo_OnCreated.cmd ui/cam_OnCreated.cmd ui/hlight--2.0_OnCreated.cmd ui/envlight_OnCreated.cmd ui/null_OnCreated.cmd ui/3Delight--VOP_OnCreated.cmd
	DESTINATION ${INSTALL_DIR}/scripts/private )

install(
	FILES ui/IncandescenceLight_OnCreated.py
	DESTINATION ${INSTALL_DIR}/scripts/obj )

install(
	FILES ui/3Delight_OnCreated.py
	DESTINATION ${INSTALL_DIR}/scripts/out )

install(
	FILES ui/3Delight_OnCreated.py
	RENAME 3DelightCloud_OnCreated.py
	DESTINATION ${INSTALL_DIR}/scripts/out )

install(
	FILES ui/SOHOparameters ui/3Delight.ds
	DESTINATION ${INSTALL_DIR}/soho/parameters )

install(
	FILES ui/3Delight.shelf
	DESTINATION ${INSTALL_DIR}/toolbar )

install(
	FILES ui/MainMenuCommon.xml
	DESTINATION ${INSTALL_DIR} )

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} "INSTDIR" "${INSTALL_DIR}/dso" )
