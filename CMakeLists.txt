
#  Specify the minimum required version of CMake to build the project.
cmake_minimum_required( VERSION 3.6 )
project( 3Delight_for_Houdini )

# Default install prefix is a 3DelightForHoudini directory in the build one.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/3DelightForHoudini" CACHE PATH "..." FORCE)
	set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT FALSE)
endif()

# Shut up 'Up-to-date' install messages.
set(CMAKE_INSTALL_MESSAGE LAZY)

if (MSVC)
	# Remove cmake's default warning level. Houdini adds /W1 which conflicts
	# and the compiler complains loudly.
	string (REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

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
	camera.cpp
	context.cpp
	cop_utilities.cpp
	creation_callbacks.cpp
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
	object_visibility_resolver.cpp
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

add_subdirectory( osl )

# 3Delight includes
target_include_directories( ${library_name} PRIVATE "${DELIGHT}/include" )

# Must use windows.h instead of boost redeclarations or there are conflicts.
target_compile_definitions( ${library_name} PRIVATE HBOOST_USE_WINDOWS_H )

# Link against the Houdini libraries, and add required include directories and
# compile definitions. Special handling needed for tbb on macOS.
target_link_libraries( ${library_name} Houdini )
IF(APPLE)
	target_link_libraries( ${library_name} tbb hboost_thread )
ENDIF()

option(INSTALL_PACKAGE_DEF "Install a package definition for the current user which will cause the built plugin to be loaded." ON)
if(INSTALL_PACKAGE_DEF)
	message(STATUS "Getting houdini install dir. This will take a while and may crash if it loads an invalid plugin.")
	houdini_get_default_install_dir(HOUDINI_USER_DIR)
	set(package_file ${HOUDINI_USER_DIR}/packages/3Delight_for_Houdini.json)
	message(STATUS "Writing package tile to ${package_file}")
	file(WRITE ${package_file} "{
\"env\" : [
	{ \"HOUDINI_PATH\" : \"${CMAKE_INSTALL_PREFIX}\" }
	]
}
")
endif()

install(
	FILES ui/ROP_3Delight.svg
	DESTINATION config/Icons )

install(
	FILES ui/ROP_3Delight.svg
	RENAME ROP_3DelightCloud.svg
	DESTINATION config/Icons )

install(
	FILES ui/vdb_no_outline.png ui/shelf_ROP_3Delight.png ui/shelf_ROP_3Delight_cloud.png ui/shelf_ROP_render_3Delight.png ui/shelf_ROP_select_3Delight.png ui/VOP_3Delight-dlHairAndFur.png ui/VOP_3Delight-dlPrincipled.png ui/VOP_3Delight-dlMetal.png ui/VOP_3Delight-dlSkin.png ui/VOP_3Delight-dlGlass.png ui/VOP_3Delight-dlSubstance.png ui/VOP_3Delight-dlColorToFloat.png ui/VOP_3Delight-dlFloatToColor.png ui/VOP_3Delight-dlRamp.png ui/VOP_3Delight-dlToon.png ui/VOP_3Delight-dlThin.png ui/VOP_3Delight-vdbVolume.png ui/OBJ_3Delight-dlIncandescenceLight.png
	DESTINATION config/Icons )

install(
	FILES ui/select_layers_ui.ui
	DESTINATION config/Applications )

install(
	FILES ui/geo_OnCreated.cmd ui/cam_OnCreated.cmd ui/hlight--2.0_OnCreated.cmd ui/envlight_OnCreated.cmd ui/null_OnCreated.cmd ui/3Delight--VOP_OnCreated.cmd ui/instance_OnCreated.cmd
	DESTINATION scripts/private )

install(
	FILES ui/3Delight-IncandescenceLight_OnCreated.py
	DESTINATION scripts/obj )

install(
	FILES ui/3Delight_OnCreated.py
	DESTINATION scripts/out )

install(
	FILES ui/3Delight_OnCreated.py
	RENAME 3DelightCloud_OnCreated.py
	DESTINATION scripts/out )

install(
	FILES ui/SOHOparameters ui/3Delight.ds
	DESTINATION soho/parameters )

install(
	FILES ui/3Delight.shelf
	DESTINATION toolbar )

install(
	FILES ui/MainMenuCommon.xml
	DESTINATION . )

# Configure several common target properties, such as its output directory.
# Has the lousy behavior of building the lib directly to its install location.
houdini_configure_target( ${library_name} "INSTDIR" "${CMAKE_INSTALL_PREFIX}/dso" )

# Add the Hydra plugin, if present, for Houdini >= 18.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HydraNSI/CMakeLists.txt"
	AND Houdini_VERSION_MAJOR GREATER_EQUAL 18)
	message(STATUS "Configuring HydraNSI plugin for Solaris")
	set(HYDRANSI_INSTALL_PREFIX "dso/usd_plugins/")
	add_subdirectory(HydraNSI)
	install(
		FILES Solaris/plugInfo.json
		DESTINATION dso/usd_plugins)
	install(
		FILES
			Solaris/HdNSIRendererPlugin_Global.ds
			Solaris/HdNSIRendererPlugin_Geometry.ds
			Solaris/HdNSIRendererPlugin_Viewport.ds
		DESTINATION soho/parameters)
endif()
