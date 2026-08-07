# 先决条件

交叉编译依赖于：

- 一个可用的"独立"构建环境（[见这里](build-standalone-unix-zh.md)）
- 一个可用的树莓派 `Buster` 环境，无论是通过 `chroot`（[见这里](rpi-buster-chroot-zh.md)）还是物理设备

# 从 Raspbian 安装中收集依赖

交叉编译工具链附带一个最小的系统"镜像"，其中不包含编译 musikcube 所需的全部第三方依赖。

要获得这些依赖，我们需要启动进入 chroot 环境并执行一个脚本来下载和收集最新版本的头部和库文件，以便将它们添加到交叉编译工具链中。

1. 进入 `chroot` 环境（或启动你的设备）
2. `apt-get update`
3. `sudo apt-get dist-upgrade`
4. `cd /build/sysroot`
5. `node /build/musikcube/script/create-crosscompile-sysroot.js`

这个过程完成后，你会得到一个名为 `sysroot.tar` 的文件。

# 安装交叉编译工具链

Debian 和 Ubuntu 自带的交叉编译工具链通常不能完全支持 `armv6`，这意味着我们无法用它们为旧款树莓派设备生成构建产物。

相反，我们将从以下 github 项目安装一个维护得更好的工具链：https://github.com/tttapa/docker-arm-cross-toolchain

1. `cd /build`
2. `cp /path/to/generated/sysroot/sysroot.tar .`
3. `node /build/musikcube/script/install-crosscompile-tools.js`

这将把交叉编译工具下载到当前目录，解压它们，然后使用 `sysroot.tar` 填充工具的环境。

# 使用交叉编译工具链编译依赖

为了便于高效地分发 musikcube 二进制文件，我们自行编译各种依赖，省略未使用的功能，并确保整个软件包是"可迁移的"，也就是说用户可以从任何地方运行它们（即它们不需要安装到 `/usr/` 或 `/usr/local/` 才能正常工作）。

1. `cd /build`
2. `CROSSCOMPILE=rpi-armv6 ./musikcube/script/build-vendor-libraries.sh`

完成后，你会得到一个 `vendor-rpi-armv6` 目录，主应用编译时会引用它。

# 构建主应用

现在我们已经有了所有依赖，是时候编译主应用了。

1. `cd /build/musikcube`
2. `CROSSCOMPILE=rpi-armv6 ./script/archive-standalone-nix.sh 3.0.2`
