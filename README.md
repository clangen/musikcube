# musikcube

a cross-platform audio engine and player written in c++.

# musikbox

a curses frontend to musikcube.

musicbox compiles and runs easily on windows, macos and linux. it also runs well on a raspberry pi with raspbian, and can be setup as a streaming audio server.

it looks something like this on windows:

![windows screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/windows.png)

and this on macos:

![osx screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/osx.png)

and on linux:

![linux screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/linux.png)

you can also stream audio from, or remote control musikbox using the `musikdroid` android app, which can be downloaded in the `releases` section above. it looks like this:

![android screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/android.png)

# raspberry pi

want to run musikbox on a raspberry pi, connected to you home stereo? [see here](https://github.com/clangen/musikcube/wiki/raspberry-pi).

# streaming server documentation

if you're interested in writing your own frontend, [api documentation is available here](https://github.com/clangen/musikcube/wiki/remote-api-documentation). the streaming server api uses a combination of websockets and vanilla http, and is included in every `musikbox` distribution.

# compiling

## windows

- grab the [Visual Studio 2017 Community Edition](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx) and install the 32-bit c++ compiler.
- clone the musikcube sources: `git clone https://github.com/clangen/musikcube.git`
- install the [32 bit version of boost 1.64](https://sourceforge.net/projects/boost/files/boost-binaries/1.64.0/boost_1_64_0-msvc-14.1-32.exe/download). ensure it shares the same parent directory with musikcube. e.g: `c:\src\musikcube` and `c:\src\boost_1_64_0` -- the project's solution will reference it via relative path.
- open `musikcube.sln` and build/run. 

## mac

you'll need [homebrew](http://brew.sh/) to install the required dependencies. 

### automatic

- `brew tap clangen/musikbox`
- `brew install musikbox`
- `musikbox`

### manual

- `brew install cmake boost libogg libvorbis flac faad2 libmicrohttpd lame`
- `git clone https://github.com/clangen/musikcube.git`
- `cd musikcube`
- `cmake .`
- `make`
- `cd bin`
- `./musikbox`

## linux

- install the following libraries and their development packages: `cmake boost libogg vorbis flac faad2 ncurses zlib asound pulse libcurl libmicrohttpd libmp3lame`
- `git clone https://github.com/clangen/musikcube.git`
- `cd musikcube`
- `cmake .`
- `make`
- `sudo make install`
- `musikbox`

## keyboard shortcuts

the hotkeys listed below can generally be used at any time; however, if an input field is focused some may not work. you can enter command mode by pressing `ESC`, which will highlight the bottom command bar and accept all hotkeys. command mode may be deactivated by pressing `ESC` again.

you may also change hotkeys by editing `~/.musikcube/hotkeys.json` and restarting the app. a hotkey tester is provided in the settings screen.

- `TAB` select next window
- `SHIFT+TAB` select previous window
- `~` switch to console view
- `a` switch to library view
- `s` switch to settings view
- `i` volume up 5%
- `k` volume down 5%
- `m` toggle volume mute
- `j` previous track
- `l` next track
- `u` back 10 seconds
- `o` forward 10 seconds
- `v` show / hide visualizer
- `r` repaint the screen
- `.` toggle repeat mode (off/track/list)
- `,` (un)shuffle play queue
- `CTRL+p` pause/resume (globally)
- `CTRL+x` stop (unload streams, free resources)
- `CTRL+d` quit 

and a couple hotkeys that are specific to the `library` view: 

- `b` show browse view
- `n` show play queue
- `f` show album/artist/genre search
- `t` show track search
- `1` browse by artist
- `2` browse by album
- `3` browse by genre
- `x` jump to playing artist/album/genre in browse view
- `SPACE` pause/resume

these only work in the `play queue` view:

- `M-s` save current queue as a playlist
- `M-l` load a previously saved playlist
- `M-x` delete a previously saved playlist
- `M-r` rename a playlist

* **note 1**: on OSX make sure you configure your terminal emulator to treat your left alt key as "+Esc" or "Meta".
* **note 2**: `ALT` is the `M`odifier key in Windows

# sdk

musikcube is built around its own SDK interfaces. they're still in the process of being refined. you can see what they look like here: https://github.com/clangen/musikcube/tree/master/src/core/sdk

# dependencies

musikcube would not be possible without the following excellent free, open source, and (in the case of some macOS and win32 APIs) non-free projects and libraries:

core:
* [boost](http://www.boost.org/)
* [sqlite](https://www.sqlite.org/)
* [utfcpp](https://github.com/nemtrif/utfcpp)
* [json.hpp](https://github.com/nlohmann/json)
* [kissfft](http://kissfft.sourceforge.net/)
* [sigslot](http://sigslot.sourceforge.net/)
* [wcwidth.c](http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c)

ui:
* [ncurses](https://www.gnu.org/software/ncurses/)
* [pdcurses (win32a variant)](https://www.projectpluto.com/win32a.htm)

decoders:
* [flac](https://xiph.org/flac/)
* [ogg/vorbis](http://www.vorbis.com/)
* [mad](http://www.underbit.com/products/mad/) + [nomad](https://github.com/cmus/cmus/tree/master/ip)
* [faad2](http://www.audiocoding.com/faad2.html)
* [exoplayer](https://github.com/google/ExoPlayer)

outputs:
* [pulseaudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)
* [alsa](https://www.alsa-project.org)
* [core audio](https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html)
* [wasapi](https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455(v=vs.85).aspx)
* [directsound](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416960(v=vs.85).aspx)
* [waveout](https://msdn.microsoft.com/en-us/library/windows/desktop/dd743876(v=vs.85).aspx)

metadata-related:
* [taglib](http://taglib.org/)
* [glide](https://github.com/bumptech/glide)

networking:
* [websocketpp](https://github.com/zaphoyd/websocketpp)
* [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
* [libcurl](https://curl.haxx.se/libcurl/)
* [libressl](https://www.libressl.org/)
* [nv-websocket-client](https://github.com/TakahikoKawasaki/nv-websocket-client)
* [okhttp](http://square.github.io/okhttp/)

miscellaneous
* [rxjava](https://github.com/ReactiveX/RxJava)
* [rxandroid](https://github.com/ReactiveX/RxAndroid)
* [recycler-fast-scroll](https://github.com/plusCubed/recycler-fast-scroll)
