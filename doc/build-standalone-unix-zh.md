# 概述

默认情况下，构建的 `musikcube` 几乎与你的 Linux 发行版和版本绑定。例如，如果你尝试把从 `Ubuntu` 编译出的二进制文件复制到 `Fedora`，它们很可能无法运行。

事实证明，要一次性构建一个能够在许多 Linux 发行版和版本上运行的单一 C++ 应用是非常有挑战性的，尤其是在涉及外部依赖时。这些困难已被反复记录和讨论过，包括但不限于：

1. `GLIBC`/`GLIBCXX` 版本不兼容
2. 所需依赖的版本不兼容
3. 所需依赖的位置不兼容（例如 `/lib` 与 `/usr/lib` 与 `/usr/local/lib`）
4. 所需依赖的文件名不兼容（例如 `libfoo.so.3` 与 `libfoo.so`）
5. 所需依赖普遍不可用（即"依赖 `foo` 在 `Ubuntu` 的 `apt` 中不存在"）

在理想情况下，我们不必担心这些问题，`musikcube` 应该可以通过所有主流 Linux 发行版的软件包管理系统获得。但在实践中并非如此。要让软件包被上游维护者接受需要付出巨大的时间和精力。例如，参见 [DebianMentorsFaq](https://wiki.debian.org/DebianMentorsFaq)。

因此，目前我们开发了一套流程，允许我们为每种 CPU 架构编译一次"通用"二进制文件，这些二进制通常可以在大多数现代 Linux 发行版上无阻碍地运行。简而言之，我们做了以下工作：

1. 使用一个 `GLIBC` 和 `GLIBCXX` 版本能够在 2018 年至今的任何 Linux 发行版上运行的操作系统进行编译。我们目前使用 `Debian Buster`。
2. 包含从源码编译的现代版本的基础依赖（`openssl`、`curl`、`ffmpeg` 等），并剔除任何未使用/多余的功能。
3. 确保应用在文件系统上是"可迁移的"；所有库都通过相对路径加载，这样你可以把 `musikcube` 目录放在任何地方，它仍然可以运行。这是通过手动重写共享库和可执行文件的 `rpath` 值来实现的。

本文档的其余部分将描述我们如何生成这些构建产物，也就是我们在 [Github 项目页面](https://github.com/clangen/musikcube/releases) 上为所有版本分发的内容。

常规构建说明可以在 [Github 项目页面，这里](https://github.com/clangen/musikcube/wiki/building) 找到，并且先决条件更少。

# 操作步骤

## 安装 `Debian Buster`

最简单的做法是安装一个 `docker` 镜像并使用它，但直接安装到物理硬件（或虚拟机）上也可以。

## 安装依赖

### 通过 `apt-get`：

- `sudo apt update`
- `sudo apt dist-upgrade`
- `sudo apt install build-essential g++ gcc git libasound2-dev libev-dev libncurses-dev libopus-dev libopus-dev libopus0 libpulse-dev libsndio-dev libssl-dev libsystemd-dev libvorbis-dev libvorbis-dev libvorbis0a libvorbisenc2 portaudio19-dev rpm wget zlib1g-dev libdbus-1-dev libudev-dev libglib2.0-dev patchelf libstdc++6-i386-cross nodejs`

### 从源码编译：

以下依赖需要从源码安装，因为发行版自带的包太旧了：

- `cmake` v3.27.4+
- `pipewire` v0.3.x（可选——仅当你想使用 `pipewire` 时）
  - 注意：`wireplumber` 是 `pipewire` 的子项目，它需要的 `glib-2.0` 版本比 `Debian Buster` 提供的更新。这没关系，因为我们只需要 `pipewire` 库。只需编辑 `meson_options.txt`，从 `session-managers` 选项中移除 `'wireplumber'` 即可。

## 准备你的构建目录

有几个构建步骤假设你将从 `/build` 目录进行构建。这可以是物理目录，也可以是链接到其他位置的符号链接。这目前是硬性要求，但将来可能不再是。抱歉。

- `cd /build`
- `git clone https://github.com/clangen/musikcube.git --recursive`

## 编译第三方依赖

如上所述，一些较大的第三方依赖是从源码编译的，会剥离未使用的功能，以确保它们尽可能小巧高效。手动操作非常麻烦，所以这一切都已通过脚本自动化完成。

- `cd /build`
- `./musikcube/script/build-vendor-libraries.sh`

这个过程可能需要一段时间，但完成后你会得到一个 `/build/vendor-${arch}` 目录，其中包含所有构建产物。`musikcube` 应用在构建过程中会引用这个目录。

## 编译 `musikcube`

现在我们已经有了所有必需的依赖，是时候编译应用本身了。如上所述，我们希望生成的二进制文件在文件系统上是可迁移的，因此必须进行一些特殊的前处理和后续处理。具体来说：我们需要收集所有依赖的副本，并在必要时重写它们的 `rpaths`。我们还需要收集并包含主题和本地化文件等资源文件，然后为分发创建 `.deb`、`.rpm` 和 `.tar.bz2` 归档。与上一步一样，所有这些复杂性都被隐藏在脚本后面。

- `cd /build/musikcube`
- `./script/archive-standalone-nix.sh x.y.z`
  - `x.y.z` 是我们希望出现在文件名中的 `musikcube` 版本。你可以使用任何值，例如 `0.0.0`
  - 构建过程会自动发现并使用上一步创建的 `/build/vendor-${arch}` 目录。

脚本完成后，你应该会在 `/build/musikcube/dist/x.y.z/` 中获得一组"可迁移"的二进制文件，它们可以在 2018 年或之后发布的多数 Linux 发行版上运行。
