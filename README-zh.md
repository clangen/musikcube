# musikcube

一个用 C++ 编写的跨平台、基于终端的音频引擎、媒体库、播放器与服务器。

musikcube 可在 Windows、macOS 和 Linux 上轻松编译运行。它也能在安装 Raspbian 的树莓派上良好运行，并且可以配置为流媒体音频服务器。

请查阅[安装指南](https://github.com/clangen/musikcube/wiki/installing)开始使用。

务必也阅读[用户指南](https://github.com/clangen/musikcube/wiki/user-guide)，其中描述了应用的导航范式，并列出了所有默认键盘快捷键。

如果你想从源码编译，[构建说明在这里](https://github.com/clangen/musikcube/wiki/building)。

# 截图

在 Windows 上它看起来像这样：

![windows screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/windows.png)

在 macOS 上：

![osx screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/osx.png)

在 Linux 上：

![linux screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/linux.png)

这里有一个演示（使用 asciinema 制作）：

[![asciicast](https://asciinema.org/a/129748.png)](https://asciinema.org/a/129748)

主 musikcube 应用在控制台（终端）中运行，同时你也可以使用 `musikdroid` 安卓应用从 musikcube 串流音频（甚至远程控制它），该应用可在上面的 releases 部分下载。它看起来像这样：

![android screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/musikcube/screenshots/android.png)

# 安装

二进制文件可在 [releases](https://github.com/clangen/musikcube/releases) 页面获取。

虽然提供了 macOS 二进制文件，你也可以通过 homebrew 安装：

- `brew install musikcube`

在 FreeBSD 上可通过以下方式安装：

- `pkg install musikcube`

在 OpenBSD 上可通过以下方式安装：

- `pkg_add install musikcube`

在 Windows 上，你可以通过 chocolatey 安装：

- `choco install musikcube`

然后使用 shell、Win+R 对话框或在开始菜单中输入 `musikcube` 或 `mcube` 来运行。

# 树莓派

musikcube 在树莓派上运行良好，可连接到你家的音响设备。[详细的设置说明请见这里](https://github.com/clangen/musikcube/wiki/raspberry-pi)。

# 编译

如果你想自己编译该项目，请查看[构建说明](https://github.com/clangen/musikcube/wiki/building)。

# 键盘快捷键

所有键盘快捷键的列表可在[用户指南](https://github.com/clangen/musikcube/wiki/user-guide)中找到。

# 流媒体服务器

musikcube 默认启用流媒体音频服务器。它运行一个用于元数据检索的 websocket 服务器（端口 7905）。还有一个 HTTP 服务器运行在端口 7906，用于向客户端提供（可选择转码的）音频数据。

**需要了解的重要一点是：开箱即用时，该服务器（以及远程 API）不应被视为在本地网络之外可以安全使用**。websocket 服务仅支持简单的密码质询，音频 HTTP 服务器只处理 Basic 认证。它不提供 SSL 或 TLS。服务器还将密码以明文形式存储在本地机器的设置文件中。

你可以使用反向代理提供 SSL 终止来部分解决这些问题。详见 [ssl-server-setup 章节](https://github.com/clangen/musikcube/wiki/ssl-server-setup)。虽然这有所改善，但你应该谨慎地通过互联网暴露这些服务。

如果你有兴趣编写自己的前端，[这里有 API 文档](https://github.com/clangen/musikcube/wiki/remote-api-documentation)。

# SDK

musikcube SDK 是一组小型的纯虚 C++ 类，外加少量枚举和常量。它们仍在精简过程中。你可以在这里查看当前的形态：https://github.com/clangen/musikcube/tree/master/src/musikcore/sdk

# 依赖

没有以下优秀的自由、开源项目与库，musikcube 就不可能实现（其中某些 macOS 和 win32 API 为非自由项目）：

| 核心                                                      | 解码器                                                        | 输出                                                                                                                                        | 元数据                                   | 网络                                                                     | 其他                                                              | UI                                                                        |
|-----------------------------------------------------------|-----------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|--------------------------------------------------------------------------------|--------------------------------------------------------------------|---------------------------------------------------------------------------|
| [sqlite](https://www.sqlite.org/)                         | [ffmpeg](https://ffmpeg.org/)                                   | [alsa](https://www.alsa-project.org)                                                                                                           | [taglib](http://taglib.org/)               | [websocketpp](https://github.com/zaphoyd/websocketpp)                          | [rxjava](https://github.com/ReactiveX/RxJava)                      | [ncurses](https://www.gnu.org/software/ncurses/)                          |
| [utfcpp](https://github.com/nemtrif/utfcpp)               | [libopenmpt](https://lib.openmpt.org/libopenmpt/)               | [pulseaudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)                                                                            | [glide](https://github.com/bumptech/glide) | [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)                   | [rxandroid](https://github.com/ReactiveX/RxAndroid)                | [pdcurses (win32a variant)](https://www.projectpluto.com/win32a.htm)      |
| [nlohmann json](https://github.com/nlohmann/json)         | [libgme](https://bitbucket.org/mpyne/game-music-emu/wiki/Home)  | [core audio](https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html) |                                            | [libcurl](https://curl.haxx.se/libcurl/)                                       | [stetho](http://facebook.github.io/stetho/)                        | [recycler-fast-scroll](https://github.com/plusCubed/recycler-fast-scroll) |
| [kissfft](http://kissfft.sourceforge.net/)                | [exoplayer](https://github.com/google/ExoPlayer)                | [wasapi](https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455(v=vs.85).aspx)                                                      |                                            | [openssl](https://www.openssl.org/)                                           | [fabric](http://fabric.io)                                         |                                                                           |
| [sigslot](http://sigslot.sourceforge.net/)                |                                                                 | [directsound](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416960(v=vs.85).aspx)                                                 |                                            | [nv-websocket-client](https://github.com/TakahikoKawasaki/nv-websocket-client) | [AndroidVideoCache](https://github.com/danikula/AndroidVideoCache) |                                                                           |
| [wcwidth.c](http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c) |                                                                 | [waveout](https://msdn.microsoft.com/en-us/library/windows/desktop/dd743876(v=vs.85).aspx)                                                     |                                            | [okhttp](http://square.github.io/okhttp/)                                      |                                                                    |                                                                           |
|                                                           |                                                                 | [pipewire](https://pipewire.org/)                                                                                                              |                                            |                                                                                |                                                                    |                                                                           |

# 许可

```
Copyright (c) 2004-2023 musikcube team

保留所有权利。

允许以源代码和二进制形式进行再分发和使用，无论是否经过
修改，前提是满足以下条件：

 * 源代码的再分发必须保留上述版权声明、
   此条件列表和以下免责声明。

 * 二进制形式的再分发必须在随分发提供的
   文档和/或其他材料中重现上述版权
   声明、此条件列表和以下免责声明。

 * 未经事先书面许可，不得使用作者姓名或其他贡献者的姓名
   来认可或推广从本软件派生的产品。

本软件由版权所有者和贡献者"按现状"提供，不承担
任何明示或暗示的保证，包括但不限于对适销性和
特定用途适用性的暗示保证。在任何情况下，版权所有者或贡献者均不
对任何直接、间接、偶然、特殊、惩戒性或
后果性损害负责（包括但不限于采购替代商品
或服务；使用、数据或利润的损失；或业务
中断），无论其原因如何，也无论依据何种责任理论，
无论是合同责任、严格责任还是侵权责任（包括疏忽或
其他），即使已被告知发生此类损害的可能性。
```
