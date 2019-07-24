#  Specify the minimum required version of CMake to build the project.
cmake_minimum_required( VERSION 3.6 )
project( 3Delight_for_Houdini )

# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
find_package( Houdini REQUIRED )

# Add a library
set( library_name 3Delight_for_Houdini )
add_library( ${library_name} SHARED
	ROP_3Delight.cpp
	VOP_ExternalOSL.cpp
	camera.cpp
	curvemesh.cpp
	exporter.cpp
	instance.cpp
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
	ui/aov.cpp
	ui/select_layers_dialog.cpp )

add_subdirectory( osl osl )

# 3Delight includes
target_include_directories( ${library_name} PRIVATE "$ENV{DELIGHT}/include"  )

# Link against the Houdini libraries, and add required include directories and
# compile definitions.
target_link_libraries( ${library_name} Houdini)

# This will get the destination directory for the installation.
houdini_get_default_install_dir( _installdir )

install(
	FILES ui/ROP_3Delight.svg
	DESTINATION ${_installdir}/config/Icons )

install(
	FILES ui/select_layers_ui.ui
	DESTINATION ${_installdir}/config/Applications )

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} )
