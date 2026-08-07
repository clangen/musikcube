# musikdroid

`musikdroid` 是一个安卓应用，用于从现有的 `musikcube` 安装（Windows、macOS 或 Linux 客户端）串流音乐，或对其进行远程控制。它使用 `kotlin` 编写。

# 构建

由于 `musikdroid` 不在 Google Play 商店中，它使用 [fabric.io](https://fabric.io) 进行崩溃报告。这使得开箱即用地构建有些困难，因为项目的 API 密钥没有签入。如果你不是开发团队成员，但仍想玩玩代码，可以这样做：

1. 从 `Application.kt` 中移除 `Fabric.with(this, Crashlytics())`
2. 从 `app/build.gradle` 中移除 `apply plugin: 'io.fabric'`

这样应该就能在没有特殊密钥的情况下在本地构建和测试了。

该项目目前使用 `Android Studio 3` 构建。

# 署名

以下图标取自 [the noun project](https://thenounproject.com)，遵循 [知识共享 3.0 许可](https://creativecommons.org/licenses/by/3.0/)
- https://thenounproject.com/search/?q=remote&i=658529
- https://thenounproject.com/iconsguru/collection/audio/?oq=audio&cidx=0&i=1052418
