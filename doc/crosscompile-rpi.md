# Prerequisites

Cross compiling depends on:

- A working "standalone" build environment ([see here](build-standalone-unix.md))
- A working Raspberry Pi `Buster` environment, either via `chroot` ([see here](rpi-buster-chroot.md)) or on a physical device

# Gather dependencies from a Raspbian install

The cross-compile toolchain ships with a minimal system "image" that does not include all of the third-party dependencies we need to compile musikcube.

To obtain these dependencies we need to boot into the chroot environment and execute a script to download and collect the latest versions of the headers and libraries so we can add them to the cross-compile toolchain.

1. Enter the `chroot` environment (or boot your device)
2. `apt-get update`
3. `sudo apt-get dist-upgrade`
4. `cd /build/sysroot`
5. `node /build/musikcube/script/create-crosscompile-sysroot.js`

After this process completes you will be left with a file called `sysroot.tar`.

# Install the cross-compile toolchain

The cross-compile toochains that ship with Debian and Ubuntu generally do not fully support `armv6`, which means we cannot use them to generate builds for older Raspberry Pi devices.

Instead, we'll install a better maintained toolchain from the following github project: https://github.com/tttapa/docker-arm-cross-toolchain

1. `cd /build`
2. `cp /path/to/generated/sysroot/sysroot.tar .`
3. `node /build/musikcube/script/install-crosscompile-tools.js`

This will download the cross-compile tools to the current directory, extract them, then use the `sysroot.tar` to populate the tools' environments.

# Use cross-compile toolchain to compile dependencies

In order to facilite easy, efficient distribution of musikcube binaries, we compile various dependencies ourselves, omitting unused features and ensuring the entire package is "relocatable," meaning users can run them from anywhere (i.e. they don't need to be installed to `/usr/` or `/usr/local/` to work properly).

1. `cd /build`
2. `CROSSCOMPILE=rpi-armv6 ./musikcube/script/build-vendor-libraries.sh`

When this has finished, you'll be left with a `vendor-rpi-armv6` directory that the main app compile will reference.

# Build the main app

Now that we have all of our dependencies available it's time to compile the main app.

1. `cd /build/musikcube`
2. `CROSSCOMPILE=rpi-armv6 ./script/archive-standalone-nix.sh 3.0.2`
