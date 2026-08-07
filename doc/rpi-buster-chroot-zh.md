
# 前言
- 转载自：https://medium.com/@crmoratelli/architectural-chroot-for-faster-compilation-and-deployment-on-raspberry-pi-76224327659d

# 安装依赖
- `sudo apt install qemu-kvm qemu-user-static binfmt-support qemu-user-static qemu-utils kpartx e2fsprogs`

# 创建一个新目录
- `mkdir rpi-buster-chroot`

# 下载镜像
- `wget https://downloads.raspberrypi.org/raspios_oldstable_lite_armhf/images/raspios_oldstable_lite_armhf-2023-05-03/2023-05-03-raspios-buster-armhf-lite.img.xz`

# 解压镜像
- `xz -d -v 2023-05-03-raspios-buster-armhf-lite.img.xz`

# 扩展镜像
- `qemu-img resize -f raw 2023-05-03-raspios-buster-armhf-lite.img 16G`

# 扩展镜像内的分区
- `fdisk 2023-05-03-raspios-buster-armhf-lite.img`
- `Command: p`   [打印分区表。记下分区 #2 的 "Start" 值。]
- `Command: d`   [删除一个分区]
- `Command: 2`   [删除 Linux 分区，应该是 #2]
- `Command: n`   [添加新分区]
- `Command: p`   [主分区]
- `Command: 2`   [分区号]
- `Command:  `   [输入第一步得到的 start 值]
- `Command:  `   [按回车接受默认值]
- `Command: N`   [*不* 移除 ext4 签名]
- `Command: w`   [将分区表写入文件]

# 创建挂载点
- `mkdir os-mount`

# 通过 loopback 挂载两个磁盘镜像分区
- `sudo kpartx -a -v 2023-05-03-raspios-buster-armhf-lite.img`

输出，注意设备名：
```
add map loop4p1 (253:0): 0 524288 linear 7:4 8192
add map loop4p2 (253:1): 0 33021952 linear 7:4 532480  <== 这才是我们真正的操作系统
```

# 将第二个分区挂载到文件系统
- `sudo mount /dev/mapper/loop4p2 ./os-mount`

# 调整 ext 文件系统大小
- `sudo /sbin/resize2fs /dev/mapper/loop4p2`

# 注入 qemu 并设置特殊挂载点
- `sudo cp /usr/bin/qemu-arm-static ./os-mount/usr/bin`
- `sudo mount -o bind /dev ./os-mount/dev`
- `sudo mount -o bind /proc ./os-mount/proc`
- `sudo mount -o bind /sys ./os-mount/sys`

# 魔法：在内核中注册 `qemu-arm-static` 为 arm 解释器
> 必须是 root，不能用 `sudo`
- `su`
- `echo ':arm:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-arm-static:' > /proc/sys/fs/binfmt_misc/register`
- `exit`

# 进入 chroot
- `sudo chroot ./os-mount /usr/bin/qemu-arm-static /usr/bin/bash`
> 如果出现 /usr/bin/bash 未找到的错误，请尝试使用 /bin/bash

# 关闭
- `sudo umount ./os-mount/dev`
- `sudo umount ./os-mount/proc`
- `sudo umount ./os-mount/sys`
- `sudo umount ./os-mount`
- `sudo kpartx -d -v 2023-05-03-raspios-buster-armhf-lite.img`
