# Cross-compile musikcube for Raspberry Pi

Use a Linux x86-64 build host with the [standalone build prerequisites](build-standalone-unix.md) and GNU `readelf` from binutils.

| Profile | Output | Optional plugin inputs |
| --- | --- | --- |
| `rpi-armv6` | 32-bit ARMv6 hard-float, for Pi 1 and Zero | Raspbian Bullseye |
| `rpi-armv8` | 32-bit ARMv8-A hard-float, for ARMv8-capable Pi models | Debian Bullseye |

Both profiles use an x-tools compiler with glibc 2.31. The build stages the pinned toolchain and dependency packages automatically. The toolchain supplies libc and its headers. The dependency packages supply headers and link libraries for the optional ALSA, PortAudio, PulseAudio, sndio, MPRIS, and PipeWire plugins.

The ARMv6 build uses the target's `libatomic.so.1`. The x-tools static archive contains ARMv7 dispatch routines that raise the output's ELF architecture to ARMv7.

## Build and package

From the directory containing the `musikcube` checkout:

```sh
node musikcube/script/build-vendor-libraries.js --compiler=rpi-armv6
node musikcube/script/build-and-archive.js --version=4.0.0 --compiler=rpi-armv6
```

Use `rpi-armv8` in both commands for that target. The vendor build writes to `vendor-rpi-armv6` or `vendor-rpi-armv8` next to the checkout and reuses downloads in `.vendor-cache`. The app build is under `musikcube/build/standalone/<profile>/`. Release files are under `musikcube/dist/<version>/`.

## Verify

The build audits vendor libraries before building the app. Before packaging, it checks ELF architecture and ABI, the glibc symbol version ceiling, and the runtime dependencies of the executables, core library, plugins, and bundled libraries. Optional plugins can use an explicitly allowed system library. Its absence disables the associated plugin.

To repeat the audit on an extracted archive:

```sh
node musikcube/script/scan-standalone.js /path/to/extracted/archive --target armv6 --glibc-max 2.31
```

Use `--target armv8` for the ARMv8 build. On a matching Pi, run `./musikcube`, index a local directory, and play a track through the intended output plugin to verify the runtime.
