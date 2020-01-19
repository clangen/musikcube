# musikcube

a cross-platform, terminal-based audio engine, library, player and server written in c++.

musikcube compiles and runs easily on windows, macos and linux. it also runs well on a raspberry pi with raspbian, and can be setup as a streaming audio server.

check out the [installation guide](https://github.com/clangen/musikcube/wiki/installing) to get up and running.

be sure to also read through a [the user guide](https://github.com/clangen/musikcube/wiki/user-guide), which describes app's navigation paradigm and lists all the default keyboard shortcuts.

# screenshots

it looks something like this on windows:

![windows screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/windows.png)

and this on macos:

![osx screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/osx.png)

and on linux:

![linux screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/linux.png)

here's a demo (made with asciinema):

[![asciicast](https://asciinema.org/a/129748.png)](https://asciinema.org/a/129748)

while the main musikcube app runs in the console, you can also stream audio from (and even remote control) musikcube using the `musikdroid` android app, which can be downloaded in the `releases` section above. it looks like this:

![android screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/android.png)

# installation

binaries are available in the [releases](https://github.com/clangen/musikcube/releases) page.

while macos binaries are provided, you can also install via homebrew as follows:

- `brew tap clangen/musikcube`
- `brew install musikcube`
- `musikcube`

on freebsd musikcube can be installed as follows:

- `pkg install musikcube`

on windows, you can install via chocolatey:

- `choco install musikcube`

then run using shell, Win+R dialog or by typing in Start Menu `musikcube` or `mcube`.

# raspberry pi

musikcube runs well on a raspberry pi, connected to your home stereo. [see here for detailed setup instructions](https://github.com/clangen/musikcube/wiki/raspberry-pi).

# compiling

if you'd like to compile the project yourself, you can check out the [build instructions](https://github.com/clangen/musikcube/wiki/building).

# keyboard shortcuts

a list of all keyboard shortcuts can be found in the [user guide](https://github.com/clangen/musikcube/wiki/user-guide)

# streaming server

musikcube ships with a streaming audio server enabled by default. it runs a websocket server on port 7905, used for metadata retrieval. an http server runs on port 7906, and is used to serve (optionally transcoded) audio data to clients.

**it's important to understand that, out of the box, the server (and remote api) should NOT be considered safe for use outside of a local network**. the websockets service only supports a simple password challenge, and the audio http server just handles Basic authorization. it does not provide ssl or tls. the server also stores the password in plain text in a settings file on the local machine.

you can fix some of this using a reverse proxy to provide ssl termination. details in the [ssl-server-setup section](https://github.com/clangen/musikcube/wiki/ssl-server-setup). while this improves things, you should exercise caution exposing these services over the internet.

if you're interested in writing your own frontend, [api documentation is available here](https://github.com/clangen/musikcube/wiki/remote-api-documentation).

# sdk

the musikcube sdk is a set of small, pure-virtual c++ classes and a handful of enums and constants. they're still in the process of being slimmed down. you can see what they currently look like here: https://github.com/clangen/musikcube/tree/master/src/core/sdk

# dependencies

musikcube would not be possible without the following excellent free, open source, and (in the case of some macos and win32 APIs) non-free projects and libraries:

| core                                                      | decoders                                                                                            | outputs                                                                                                                                        | metadata                                   | networking                                                                     | miscellaneous                                                      | ui                                                                        |
|-----------------------------------------------------------|-----------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|--------------------------------------------------------------------------------|--------------------------------------------------------------------|---------------------------------------------------------------------------|
| [boost](http://www.boost.org/)                            | [flac](https://xiph.org/flac/)                                                                      | [alsa](https://www.alsa-project.org)                                                                                                           | [taglib](http://taglib.org/)               | [websocketpp](https://github.com/zaphoyd/websocketpp)                          | [rxjava](https://github.com/ReactiveX/RxJava)                      | [ncurses](https://www.gnu.org/software/ncurses/)                          |
| [sqlite](https://www.sqlite.org/)                         | [ogg/vorbis](http://www.vorbis.com/)                                                                | [pulseaudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)                                                                            | [glide](https://github.com/bumptech/glide) | [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)                   | [rxandroid](https://github.com/ReactiveX/RxAndroid)                | [pdcurses (win32a variant)](https://www.projectpluto.com/win32a.htm)      |
| [utfcpp](https://github.com/nemtrif/utfcpp)               | [mad](http://www.underbit.com/products/mad/) + [nomad](https://github.com/cmus/cmus/tree/master/ip) | [core audio](https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html) |                                            | [libcurl](https://curl.haxx.se/libcurl/)                                       | [stetho](http://facebook.github.io/stetho/)                        | [recycler-fast-scroll](https://github.com/plusCubed/recycler-fast-scroll) |
| [nlohmann json](https://github.com/nlohmann/json)              | [faad2](http://www.audiocoding.com/faad2.html)                                                      | [wasapi](https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455(v=vs.85).aspx)                                                      |                                            | [libressl](https://www.libressl.org/)                                          | [fabric](http://fabric.io)                                         |                                                                           |
| [kissfft](http://kissfft.sourceforge.net/)                | [exoplayer](https://github.com/google/ExoPlayer)                                                    | [directsound](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416960(v=vs.85).aspx)                                                 |                                            | [nv-websocket-client](https://github.com/TakahikoKawasaki/nv-websocket-client) | [AndroidVideoCache](https://github.com/danikula/AndroidVideoCache) |                                                                           |
| [sigslot](http://sigslot.sourceforge.net/)                |                                                                                                     | [waveout](https://msdn.microsoft.com/en-us/library/windows/desktop/dd743876(v=vs.85).aspx)                                                     |                                            | [okhttp](http://square.github.io/okhttp/)                                      |                                                                    |                                                                           |
| [wcwidth.c](http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c) |                                                                                                     |                                                                                                                                                |                                            |                                                                                |                                                                    |                                                                           |

# license

```
Copyright (c) 2004-2019 musikcube team

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

 * Neither the name of the author nor the names of other contributors may
   be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```
