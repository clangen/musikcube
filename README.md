# musikcube

a cross-platform audio engine and player written in C++.

# musikbox

a curses frontend to musikcube.

musicbox compiles and runs easily on Windows, OSX and Linux.

it looks something like this on windows:

![windows screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/windows.png)

and this on osx:

![osx screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/osx.png)

and linux:

![linux screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/linux.png)

# compiling

## windows

- grab the [Visual Studio 2015 Community Edition](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx) and install the C++ compiler and tools. the 32-bit compiler is currently used. 
- clone the musikcube sources: `git clone https://github.com/clangen/musikcube.git`
- install the [32 bit version of boost 1.60](https://sourceforge.net/projects/boost/files/boost-binaries/1.60.0/boost_1_60_0-msvc-14.0-32.exe/download). ensure it shares the same parent directory with musikcube. e.g: `c:\src\musikcube` and `c:\src\boost_1_60_0` -- the project's solution will reference it via relative path.
- open `musikcube.sln` and build/run. 

## mac

you'll need [homebrew](http://brew.sh/) to install the required dependencies. 

### automatic

- `brew tap clangen/musikbox`
- `brew install musikbox`
- `musikbox`

### manual

- `brew install cmake boost libogg libvorbis flac mpg123 faad2`
- `git clone https://github.com/clangen/musikcube.git`
- `cd musikcube`
- `cmake .`
- `make`
- `cd bin`
- `./musikbox`

## linux

- install the following libraries and their development packages: `cmake boost libogg libfftw3 vorbis flac mpg123 faad2 ncurses zlib asound`
- `git clone https://github.com/clangen/musikcube.git`
- `cd musikcube`
- `cmake .`
- `make`
- `sudo make install`
- `musikbox`

## keyboard shortcuts

the hotkeys listed below can generally be used at any time; however, if an input field is focused some may not work. you can enter command mode by pressing `ESC`, which will highlight the bottom command bar and accept all hotkeys. command mode may be deactivated by pressing `ESC` again.

you may also change hotkeys by editing `~/.mC2/hotkeys.json` and restarting the app. a hotkey tester is provided in the settings screen.

- `TAB` select next window
- `SHIFT+TAB` select previous window
- `~` switch to console view
- `a` switch to library view
- `s` switch to settings view
- `i` volume up 5%
- `k` volume down 5%
- `j` previous track
- `l` next track
- `u` back 10 seconds
- `o` forward 10 seconds
- `r` repaint the screen
- `.` toggle repeat mode (off/track/list)
- `,` (un)shuffle play queue
- `CTRL+p` pause/resume (globally)
- `CTRL+x` stop (unload streams, free resources)
- `CTRL+d` quit 

and a couple hotkeys that are specific to the library view: 

- `b` show browse view
- `n` show play queue
- `f` show album/artist/genre search
- `t` show track search
- `1` browse by artist
- `2` browse by album
- `3` browse by genre
- `m` jump to playing artist/album/genre in browse view
- `SPACE` pause/resume

*important*: on OSX make sure you configure your terminal emulator to treat your left alt key as "+Esc" or "Meta".

# sdk

musikcube is built around its own SDK interfaces. they're still in the process of being cleaned up and slimmed down. you can see what they look like here: https://github.com/clangen/musikcube/tree/master/src/core/sdk
