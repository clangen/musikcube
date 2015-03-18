# Getting started with the development #
## Windows ##
To work on mc2, you will need a copy of Visual Studio. We mainly use Visual C++ 2005. Unfortunately the 2005 Express Edition does not support native windows application development out of the box. It might work together with the Platform SDK, but you are on your own there. If you do not have Visual Studio 200X Standard or better, you can download the free Visual C++ 2008 Express Edition from [microsoft.com](http://www.microsoft.com).

To access the source code repository, I recommend using [RapidSVN](http://rapidsvn.tigris.org/). With Visual C++ 2005 you should be able to compile right after the initial checkout. With other versions of Visual C++, you will have to download the boost libraries. You can find an installer for precompiled binaries at http://www.boostpro.com/products/free.

The installer should be mostly self explanatory. Note that it requires an internet connection during setup. The default variants you need are the Multithread variants (normal and debug, static runtime). Each variant is about 150 MB, so you can take a coffee break during download and installation. After the installation is complete, load the mc2 solution file into Visual Studio. You  have to add the boost directory to your include paths to be able to build mc2.

The solution you probably want to work on is mc2+plugins.sln.


deprecated:
If you're going to test current trunk of mC2 you currently need to manually add the synchronize path.
  * Build mC2 and the plugins.
  * Start mC2.exe and then close it again so that the database is created.
  * Locate the database in C:\Documents and Settings\%username%\Application Data\mC2\local
  * Open the local\_musik.db using sqlite.exe (download from http://www.sqlite.org)
  * In the sqlite prompt write  `INSERT INTO paths (path) VALUES ('C:/MyMusikPath/');` and press enter.
  * Close sqlite.exe and restart mC2.exe
  * mC2 will now start indexing the files in the added path. You can click on the genre/artist/album views "All" to refresh.

## Linux ##
Not everything is working yet in Linux, but the core library and some plugins do compile and run, as does the basic command line app musikSquare.

To compile, you will need Boost, and cmake. For the various plugins you will also need taglib, libmpg123, libflac and libesd (ESD is currently the only working output plugin on Linux).

The following commands will compile musikCube

```
cmake -G 'Unix Makefiles' <path-to-musikcube-root-directory>
make
```
CMake can also generate project files for KDevelop, Eclipse and Code::Blocks IDEs. Just run cmake with no arguments for details

Any questions about the linux version, email me at urioxis AT gmail DOT com