# musikcube-websockets
a playback remote for `musikcube` using web sockets. 

`musikcube` for windows, macOS, and linux are supported. `musikdroid` is a reference implementation for clients.

[api documentation can be found here](https://github.com/clangen/musikcube/wiki/remote-api-documentation).

the code is somewhat ugly, but it works pretty well. i currently have it running 24/7 on a raspberry pi with an [iqaudio dac+](http://iqaudio.co.uk/audio/8-pi-dac-0712411999643.html), paired with a first generation moto g as a remote. 

the backend plugin uses [websocketpp](https://github.com/zaphoyd/websocketpp). the android client uses [nv-websocket-client](https://github.com/TakahikoKawasaki/nv-websocket-client).
