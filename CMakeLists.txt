#  Specify the minimum required version of CMake to build the project.
cmake_minimum_required( VERSION 3.6 )
project( ROP_3Delight )

# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
find_package( Houdini REQUIRED )

# Add a library
set( library_name ROP_3Delight )
add_library( ${library_name} SHARED
	ROP_3Delight.cpp
	camera.cpp
	exporter.cpp
	light.cpp
	mesh.cpp
	pointmesh.cpp
	null.cpp
	scene.cpp
	shader_library.cpp
	utilities.cpp )

# 3Deligt includes
target_include_directories( ${library_name} PRIVATE "$ENV{DELIGHT}/include"  )

# Link against the Houdini libraries, and add required include directories and
# compile definitions.
target_link_libraries( ${library_name} Houdini )

configure_file(
	ROP_3Delight.svg $ENV{HIH}/config/Icons/ROP_3Delight.svg COPYONLY )

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} )
