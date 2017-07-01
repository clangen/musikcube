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

# installation

binaries are available in the [releases](https://github.com/clangen/musikcube/releases) page. 

while macos binaries are provided, you can also install via homebrew as follows:

- `brew tap clangen/musikbox`
- `brew install musikbox`
- `musikbox`

# raspberry pi

musikbox runs well on a raspberry pi, connected to you home stereo. [see here for detailed setup instructions](https://github.com/clangen/musikcube/wiki/raspberry-pi).

# compiling

if you'd like to compile the project yourself, you can check out the [build instructions](https://github.com/clangen/musikcube/wiki/building).

# streaming server

**it's important to understand that, out of the box, the remote api should NOT be considered safe for use outside of a local network**. the websockets service only supports a simple password challenge, and the audio http server just handles Basic authorization. it does not provide ssl or tls.

the server also stores the password in plain text in a settings file on the local machine.

you can fix some of this using a reverse proxy to provide ssl termination. details in the [ssl-server-setup section](https://github.com/clangen/musikcube/wiki/ssl-server-setup). while this improves things, you should exercise caution exposing these services over the internet.

if you're interested in writing your own frontend, [api documentation is available here](https://github.com/clangen/musikcube/wiki/remote-api-documentation). the streaming server api uses a combination of websockets and vanilla http, and is included in every `musikbox` distribution.


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
- `4` browse by album artist
- `5` browse by playlist
    - `M-n` create a new empty playlist
    - `M-s` save the currently selected playlist
    - `DEL` (`BACKSPACE` on macos) in the playlists pane: delete the selected playlist
    - `M-r` rename the selected playlist
    - `M-UP` (`CTRL-UP` on macos) move the selected track up
    - `M-DOWN` (`CTRL-DOWN` on macos) move the selected track down
    - `DEL` (`BACKSPACE` on macos) in the tracks pane: delete the selected track
- `x` jump to playing artist/album/genre in browse view
- `M-ENTER` show a context menu for the currently selected item (album, artist, genre, track)
- `SPACE` pause/resume

you can manipulate the play `play queue` as follows:

- `M-s` save current queue as a playlist
- `M-l` load a previously saved playlist
- `M-x` delete a previously saved playlist
- `M-r` rename a playlist
- `M-UP` (`CTRL-UP` on macos) move the selected track up
- `M-DOWN` (`CTRL-DOWN` on macos) move the selected track down
- `DEL` (`BACKSPACE` on macos) delete the selected track

a couple important **notes**:

- on macos make sure you configure your terminal emulator to treat your left alt key as "+Esc" or "Meta"
- `ALT` is the `M`odifier key in the windows build

# sdk

the musikcube sdk is a set of small, pure-virtual c++ classes and a handful of enums and constants. they're still in the process of being refined. you can see what they currently look like here: https://github.com/clangen/musikcube/tree/master/src/core/sdk

# dependencies

musikcube would not be possible without the following excellent free, open source, and (in the case of some macos and win32 APIs) non-free projects and libraries:

| core                                                      | decoders                                                                                            | outputs                                                                                                                                        | metadata                                   | networking                                                                     | miscellaneous                                                             | ui                                                                   | 
|-----------------------------------------------------------|-----------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|--------------------------------------------------------------------------------|---------------------------------------------------------------------------|----------------------------------------------------------------------| 
| [boost](http://www.boost.org/)                            | [flac](https://xiph.org/flac/)                                                                      | [alsa](https://www.alsa-project.org)                                                                                                           | [taglib](http://taglib.org/)               | [websocketpp](https://github.com/zaphoyd/websocketpp)                          | [rxjava](https://github.com/ReactiveX/RxJava)                             | [ncurses](https://www.gnu.org/software/ncurses/)                     | 
| [sqlite](https://www.sqlite.org/)                         | [ogg/vorbis](http://www.vorbis.com/)                                                                | [pulseaudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)                                                                            | [glide](https://github.com/bumptech/glide) | [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)                   | [rxandroid](https://github.com/ReactiveX/RxAndroid)                       | [pdcurses (win32a variant)](https://www.projectpluto.com/win32a.htm) | 
| [utfcpp](https://github.com/nemtrif/utfcpp)               | [mad](http://www.underbit.com/products/mad/) + [nomad](https://github.com/cmus/cmus/tree/master/ip) | [core audio](https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html) |                                            | [libcurl](https://curl.haxx.se/libcurl/)                                       | [recycler-fast-scroll](https://github.com/plusCubed/recycler-fast-scroll) |                                                                      | 
| [json.hpp](https://github.com/nlohmann/json)              | [faad2](http://www.audiocoding.com/faad2.html)                                                      | [wasapi](https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455(v=vs.85).aspx)                                                      |                                            | [libressl](https://www.libressl.org/)                                          |                                                                           |                                                                      | 
| [kissfft](http://kissfft.sourceforge.net/)                | [exoplayer](https://github.com/google/ExoPlayer)                                                    | [directsound](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416960(v=vs.85).aspx)                                                 |                                            | [nv-websocket-client](https://github.com/TakahikoKawasaki/nv-websocket-client) |                                                                           |                                                                      | 
| [sigslot](http://sigslot.sourceforge.net/)                |                                                                                                     | [waveout](https://msdn.microsoft.com/en-us/library/windows/desktop/dd743876(v=vs.85).aspx)                                                     |                                            | [okhttp](http://square.github.io/okhttp/)                                      |                                                                           |                                                                      | 
| [wcwidth.c](http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c) |                                                                                                     |                                                                                                                                                |                                            |                                                                                |                                                                           |                                                                      | 
