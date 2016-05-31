# musikcube

a cross-platform audio engine written in C++.

# musikbox

an ncurses frontend to musikcube.

musicbox runs and compiles easily on Windows and OSX (and Linux soon).

# compiling

## windows

- install the [Visual Studio 2015 Community Edition](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx) and ensure the C++ compiler and tools are installed.
- clone the musikcube sources: `git clone https://github.com/clangen/musikcube.git`
- install the [32 bit version of boost 1.60](https://sourceforge.net/projects/boost/files/boost-binaries/1.60.0/boost_1_60_0-msvc-14.0-32.exe/download). ensure it shares the same parent directory with musikcube. e.g: `c:\src\musikcube` and `c:\src\boost_1_60_0` -- the project's solution will reference it via relative paths.
- open `audioengine.sln` and build/run. 

## mac

you'll need [homebrew](http://brew.sh/) to install the required dependenies. 

- `brew install cmake boost libogg libvorbis flac mpg123 taglib`
- `git clone https://github.com/clangen/musikcube.git`
- `cd musikcube`
- `cmake .`
- `make`
- `cd bin`
- `./musikcube`

# sdk

if you know C++ you can checkout the sdk interfaces here: https://github.com/clangen/musikcube/tree/master/src/core/sdk
