# musikcube server plugin

a playback remote and streaming audio server for `musikcube` using web sockets and vanilla http.

* [`musikcube`](https://github.com/clangen/musikcube) for windows, macOS, linux and freebsd are all supported. 
* [`musikdroid`](https://github.com/clangen/musikcube/tree/master/src/musikdroid) is a client implementation written for android.
* [api documentation can be found here](https://github.com/clangen/musikcube/wiki/remote-api-documentation).

the code is somewhat ugly, but it works pretty well. i currently have it running 24/7 on a raspberry pi with an [iqaudio dac+](http://iqaudio.co.uk/audio/8-pi-dac-0712411999643.html), paired with a first generation moto g as a remote. 

the backend plugin uses [`websocketpp`](https://github.com/zaphoyd/websocketpp) and [`libmicrohttpd`](https://www.gnu.org/software/libmicrohttpd/). the android client uses [`nv-websocket-client`](https://github.com/TakahikoKawasaki/nv-websocket-client) and [`exoplayer`](https://github.com/google/ExoPlayer).
