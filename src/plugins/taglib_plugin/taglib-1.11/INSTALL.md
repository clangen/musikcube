TagLib Installation
===================

TagLib uses the CMake build system. As a user, you will most likely want to
build TagLib in release mode and install it into a system-wide location.
This can be done using the following commands:

    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release .
    make
    sudo make install

In order to build the included examples, use the `BUILD_EXAMPLES` option:

    cmake -DBUILD_EXAMPLES=ON [...]

See http://www.cmake.org/cmake/help/runningcmake.html for generic help on
running CMake.

Mac OS X
--------

On Mac OS X, you might want to build a framework that can be easily integrated
into your application. If you set the BUILD_FRAMEWORK option on, it will compile
TagLib as a framework. For example, the following command can be used to build
an Universal Binary framework with Mac OS X 10.4 as the deployment target:

    cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_FRAMEWORK=ON \
      -DCMAKE_C_COMPILER=/usr/bin/gcc-4.0 \
      -DCMAKE_CXX_COMPILER=/usr/bin/c++-4.0 \
      -DCMAKE_OSX_SYSROOT=/Developer/SDKs/MacOSX10.4u.sdk/ \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.4 \
      -DCMAKE_OSX_ARCHITECTURES="ppc;i386;x86_64"

For a 10.6 Snow Leopard static library with both 32-bit and 64-bit code, use:

    cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.6 \
      -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX="<folder you want to build to>"

After `make`, and `make install`, add `libtag.` to your XCode project, and add
the include folder to the project's User Header Search Paths.

Windows
-------

It's Windows ... Systems vary!
This means you need to adjust things to suit your system, especially paths.

Tested with:
* Microsoft Visual Studio 2010, 2015, 2017
* Microsoft C++ Build Tools 2015, 2017 (standalone packages not requiring Visual Studio)
* Gcc by mingw-w64.sf.net v4.6.3 (Strawberry Perl 32b)
* MinGW32-4.8.0

Requirements:
* Tool chain, build environment, whatever ya want to call it ...
     Installed and working.
* CMake program. (Available at: www.cmake.org)
     Installed and working.

Optional:
*  Zlib library.
     Available in some tool chains, not all.
     Search the web, take your choice.

Useful configuration options used with CMake (GUI and/or command line):
  Any of the `ZLIB_` variables may be used at the command line, `ZLIB_ROOT` is only
  available on the command line.
  
  | option               | description |
   ---------------------| ------------|
  `ZLIB_ROOT=`         | Where to find ZLib's root directory. Assumes parent of: `\include` and `\lib.`|
   `ZLIB_INCLUDE_DIR=`  | Where to find ZLib's Include directory.|
   `ZLIB_LIBRARY=`      | Where to find ZLib's Library.
   `ZLIB_SOURCE=`       | Where to find ZLib's Source Code. Alternative to `ZLIB_INCLUDE_DIR` and `ZLIB_LIBRARY`.
   `CMAKE_INSTALL_PREFIX=` | Where to install Taglib. |
   `CMAKE_BUILD_TYPE=`  | Release, Debug, etc ... (Not available in MSVC) |

The easiest way is at the command prompt (Visual C++ command prompt for MSVS users – batch file and/or shortcuts are your friends).

1.  **Build the Makefiles:**

    Replace "GENERATOR" with your needs.
    * For MSVS: `Visual Studio XX YYYY`, e.g. `Visual Studio 14 2015`.
    
      **Note**: As Visual Studio 2017 supports CMake, you can skip this step and open the taglib
      folder in VS instead.
    * For MinGW: `MinGW Makefiles`
       
          C:\GitRoot\taglib> cmake -G "GENERATOR"  -DCMAKE_INSTALL_PREFIX=C:\Libraries\taglib

    Or use the CMake GUI:
    1. Open CMake GUI.
    2. Set paths: *Where is the source code* and *Where to build the binaries*.

       In the example, both would be: `C:\GitRoot\taglib`
    3. Tick: Advanced
    4. Select: Configure
    5. Select: Generator
    6. Tick: Use default native compilers
    7. Select: Finish
      Wait until done.
    8. If using ZLib, Scroll down.
    (to the bottom of the list of options ... should go over them all)
       1. Edit: `ZLIB_INCLUDE_DIR`
       2. Edit: `ZLIB_LIBRARY`
    9. Select: Generate

2.  **Build the project**
    * MSVS:

          C:\GitRoot\taglib> msbuild all_build.vcxproj /p:Configuration=Release
      OR (Depending on MSVS version or personal choice)

          C:\GitRoot\taglib> devenv all_build.vcxproj /build Release
      OR in the MSVS GUI:
      1. Open MSVS.
      2. Open taglib solution.
      3. Set build type to: Release (look in the tool bars)
      2. Hit F7 to build the solution. (project)
    * MinGW:

          C:\GitRoot\taglib> gmake

      OR (Depending on MinGW install)
      
          C:\GitRoot\taglib> mingw32-make



3.  **Install the project**
  
    (Change `install` to `uninstall` to uninstall the project)
    * MSVS:

          C:\GitRoot\taglib> msbuild install.vcxproj
      OR (Depending on MSVC version or personal choice)

          C:\GitRoot\taglib> devenv install.vcxproj
      
      Or in the MSVS GUI:
        1. Open project.
        2. Open Solution Explorer.
        3. Right Click: INSTALL
        4. Select: Project Only
        5. Select: Build Only INSTALL
    * MinGW:

          C:\GitRoot\taglib> gmake install
      OR (Depending on MinGW install)
          
          C:\GitRoot\taglib> mingw32-make install


To build a static library, set the following two options with CMake:

    -DBUILD_SHARED_LIBS=OFF -DENABLE_STATIC_RUNTIME=ON

Including `ENABLE_STATIC_RUNTIME=ON` indicates you want TagLib built using the
static runtime library, rather than the DLL form of the runtime.

Unit Tests
----------

If you want to run the test suite to make sure TagLib works properly on your
system, you need to have cppunit installed. To build the tests, include
the option `-DBUILD_TESTS=on` when running cmake.

The test suite has a custom target in the build system, so you can run
the tests using make:

    make check
