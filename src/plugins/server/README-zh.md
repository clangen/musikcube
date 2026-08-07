# musikcube 服务器插件

一个用于 `musikcube` 的播放远程控制和流媒体音频服务器，基于 web sockets 和纯 http。

* 支持 `musikcube`（[链接](https://github.com/clangen/musikcube)）的 Windows、macOS、Linux 和 FreeBSD 版本。
* [`musikdroid`](https://github.com/clangen/musikcube/tree/master/src/musikdroid) 是一个为安卓编写的客户端实现。
* [API 文档可以在这里找到](https://github.com/clangen/musikcube/wiki/remote-api-documentation)。

代码有些粗糙，但运行得很好。我目前让它在树莓派上 24/7 运行，配备一个 [iqaudio dac+](http://iqaudio.co.uk/audio/8-pi-dac-0712411999643.html)，与第一代 moto g 配对作为遥控器。

后端插件使用 [`websocketpp`](https://github.com/zaphoyd/websocketpp) 和 [`libmicrohttpd`](https://www.gnu.org/software/libmicrohttpd/)。安卓客户端使用 [`nv-websocket-client`](https://github.com/TakahikoKawasaki/nv-websocket-client) 和 [`exoplayer`](https://github.com/google/ExoPlayer)。
