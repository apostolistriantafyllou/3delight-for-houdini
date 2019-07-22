# 3Delight for Houdini

## Building

Before building on any platform, Houdin and 3Delight (last version) ha to be
installed.  Then the Houdini setup script has to be executed.
For example:
```csh
pushd $HFS
source houdini_setup_csh
popd
```

### Linux

```cmake
cd 3delight-for-houdini
mkdir build
cd build
cmake ..
make -j4 install
```

You will need to re-start Houdini to see the ROP.

Here is an example output of a successfull build. Note how some shaders and some
UI elements are being copied into the the same directory where the ROP is
installed.

```
[  4%] Built target hlight
[ 14%] Built target texture__2_0
[ 14%] Built target principledshader__2_0
[ 19%] Building CXX object CMakeFiles/3Delight_for_Houdini.dir/shader_library.cpp.o
[ 23%] Building CXX object CMakeFiles/3Delight_for_Houdini.dir/vop.cpp.o
[ 28%] Linking CXX shared library /users1/aghiles/houdini17.5/dso/3Delight_for_Houdini.so
[100%] Built target 3Delight_for_Houdini
Install the project...
-- Install configuration: ""
-- Up-to-date: /users1/aghiles/houdini17.5/config/Icons/ROP_3Delight.svg
-- Up-to-date: /users1/aghiles/houdini17.5/config/Applications/select_layers_ui.ui
-- Installing: /users1/aghiles/houdini17.5/osl/hlight.oso
-- Installing: /users1/aghiles/houdini17.5/osl/principledshader__2_0.oso
-- Installing: /users1/aghiles/houdini17.5/osl/texture__2_0.oso
```


### macOS

On macOS, you will need to install TBB and have apply a small patch to CMakeLists.txt.
After that the same instructions as for Linux will do the job. Note that there
is no reason for the plugin to require TBB and we ware working to resolve this
issue with SideFx.

In the following patch, we added a path to my local TBB installation and
added it to the libraries to link with (1 line to add, 1 line to change). Note
that the path to TBB will be different in your case.

```patch
diff --git a/CMakeLists.txt b/CMakeLists.txt
index 8ab3989..82e1f5e 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -7,6 +7,9 @@ project( 3Delight_for_Houdini )
 list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
 find_package( Houdini REQUIRED )

+link_directories($ENV{DELIGHT_ROOT}/external-build/darwin-x86_64-libc++/tbb/lib)
+
+
 # Add a library
 set( library_name 3Delight_for_Houdini )
 add_library( ${library_name} SHARED
@@ -35,7 +38,7 @@ target_include_directories( ${library_name} PRIVATE "$ENV{DELIGHT}/include"  )

 # Link against the Houdini libraries, and add required include directories and
 # compile definitions.
-target_link_libraries( ${library_name} Houdini )
+target_link_libraries( ${library_name} Houdini tbb )

 # This will get the destination directory for the installation.
 houdini_get_default_install_dir( _installdir )
```

### Windows

Untested.

## More Detailed Explanation About What is What

To build the plug-in, you neeed:

1. A Houdini installation (17.5 and higher but lesser versions might work too).
2. A 3Delight Installation.

The Houdini installation contains all the  Houdini includes and librarires
needed for the compilation and the linking of the ROP.

The 3Delight installation is used in the following way:

1. During compilation, we need files such as <nsi.hpp> which come with the 3Delight package.
2. During compilation, CMake file will execute `oslc` executable included with 3Delight to compile OSL shaders that come with the plug-in.
3. During installation, the CMake file will copy 3Delight materials alongside the ROP.
4. During execution, the ROP will do a dynamic link with 3Delight library. Note that the library is *not* linked during plug-in complilation.

## Directory Structure and File Names

We keep a simple directory structure that is efficient to work with:

* The source directory of the plug-in contains code responsible of initializing the ROP and connverting the scene to NSI. 
* The `ui/` subdirectory contains things that are mostly related to UI. 

The file names are lower case and correpond to what they do in the simplest for
possible, without any perfixes. If you are looking for the exporter of the polygon mesh,
look for `polygonmesh.cpp` and not `exportpolygonmesh.cpp` (we know are exporting,
no need to repeat this again and again.

The scene.cpp file contains the high level code for scene conversion to NSI.
