
# Forward
- stolen from here: https://medium.com/@crmoratelli/architectural-chroot-for-faster-compilation-and-deployment-on-raspberry-pi-76224327659d

# Install dependencies
- `sudo apt install qemu-kvm qemu-user-static binfmt-support qemu-user-static qemu-utils kpartx e2fsprogs`

# Create a new directory
- `mkdir rpi-buster-chroot`

# Download image
- `wget https://downloads.raspberrypi.org/raspios_oldstable_lite_armhf/images/raspios_oldstable_lite_armhf-2023-05-03/2023-05-03-raspios-buster-armhf-lite.img.xz`

# Extract image
- `xz -d -v 2023-05-03-raspios-buster-armhf-lite.img.xz`

# Expand the image
- `qemu-img resize -f raw 2023-05-03-raspios-buster-armhf-lite.img 16G`

# Expand the partition within the image
- `fdisk 2023-05-03-raspios-buster-armhf-lite.img`
- `Command: p`   [prints the partition table. note partition #2's "Start" value.]
- `Command: d`   [delete a partition]
- `Command: 2`   [delete the Linux partition, which should be #2]
- `Command: n`   [add new partition]
- `Command: p`   [primary partition]
- `Command: 2`   [partition number]
- `Command:    ` [enter start value from first step]
- `Command:    ` [press enter to accept default value]
- `Command: N`   [to *NOT* remove the ext4 signature]
- `Command: w`   [write table to file]

# Create a mount point
- `mkdir os-mount`

# Mount both disk imagen partitions via loopback
- `sudo kpartx -a -v 2023-05-03-raspios-buster-armhf-lite.img`

output, note device names:
```
add map loop4p1 (253:0): 0 524288 linear 7:4 8192
add map loop4p2 (253:1): 0 33021952 linear 7:4 532480  <== THIS IS OUR ACTUAL OS
```

# Mount the second partition to the filesystem
- `sudo mount /dev/mapper/loop4p2 ./os-mount`

# Resize the ext filesystem
- `sudo /sbin/resize2fs /dev/mapper/loop4p2`

# Inject qemu and setup special mount points
- `sudo cp /usr/bin/qemu-arm-static ./os-mount/usr/bin`
- `sudo mount -o bind /dev ./os-mount/dev`
- `sudo mount -o bind /proc ./os-mount/proc`
- `sudo mount -o bind /sys ./os-mount/sys`

# Magic: register `qemu-arm-static` as the arm interpreter in the kernel
> Must be root, no `sudo`
- `su`
- `echo ':arm:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-arm-static:' > /proc/sys/fs/binfmt_misc/register`
- `exit`

# Enter the chroot
- `sudo chroot ./os-mount`

# Shutdown
- `sudo umount ./os-mount/dev`
- `sudo umount ./os-mount/proc`
- `sudo umount ./os-mount/sys`
- `sudo umount ./os-mount`
- `sudo kpartx -d -v 2023-05-03-raspios-buster-armhf-lite.img`
