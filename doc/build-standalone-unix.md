# Overview

By default when you build `musikcube` it's almost exclusively tied to your Linux distribution and version. If you try to copy the binaries compiled from `Ubuntu` to `Fedora`, for example, they probably won't work.

It turns out that building a single C++ app once that works across many Linux distributions and versions can be very challenging, especially when there are external dependencies involved. The difficulties have been documented and discussed countless times and include, but are not limited to:

1. Incompatible `GLIBC`/`GLIBCXX` versions
2. Incompatible versions of required dependencies
3. Incompatible locations of required dependencies (e.g. `/lib` vs `/usr/lib` vs `/usr/local/lib`)
4. Incompatible filenames for required depdencies (eg `libfoo.so.3` vs `libfoo.so`)
5. General unavailability of required dependencies (i.e. "dependency `foo` doesn't exist in `Ubuntu`'s `apt`")

In an ideal world we wouldn't have to worry about these problems, and `musikcube` would be available via all major Linux distribution package management systems. In practice, however, it's not. It can take tremendous time and effort to get packages accepted upstream by maintainers. For example, see the [DebianMentorsFaq](https://wiki.debian.org/DebianMentorsFaq).

So, for now, we've developed a process that allows us to compile "generic" binaries once per CPU architecture that should generally work across most modern Linux distributions without hassle. In short, we do the following:

1. Compile using an operating system with versions of `GLIBC` and `GLIBCXX` that should work on any Linux distribution from 2018 to now. We currently use `Debian Buster`.
2. Include modern versions of fundamental dependencies (`openssl`, `curl`, `ffmpeg` and others), compiled from source, omitting any unused/extraneous functionality.
3. Ensure the app is "relocatable" on the filesystem; all libraries are loaded via relative paths so you can put the `musikcube` directory wherever you want and it'll still run. This is done by manually rewriting `rpath` values for shared libraries and executables where necessary.

The rest of this document will describe how we produce these builds, which are what we distribute on our [Github project page](https://github.com/clangen/musikcube/releases) for all releases.

Regular build instructions can be found on the [Github project page, here](https://github.com/clangen/musikcube/wiki/building), and have fewer weird prerequisites.

# Instructions

## Install `Debian Buster`

It's probably easiest to just install a `docker` image and use that, but an install straight to physical hardware (or a virtual machine) will work just fine.

## Install dependencies

### From `apt-get`:

- `sudo apt update`
- `sudo apt dist-upgrade`
- `sudo apt install build-essential g++ gcc git libasound2-dev libev-dev libncurses-dev libpulse-dev libsndio-dev libssl-dev libsystemd-dev portaudio19-dev rpm wget pkg-config zlib1g-dev libdbus-1-dev libudev-dev libglib2.0-dev patchelf libstdc++6-i386-cross`

Install a Node.js runtime that runs on Buster, such as Node.js 16. The vendor script requires Node.js 14.14 or later.

### Compile from source:

Install the following from source, as the distro-provided packages are too old:

- `cmake` v3.27.4+
- `pipewire` v0.3.x (optional -- only if you want to use `pipewire`)
  - Note: `wireplumber`, a `pipewire` sub-project, requires a version of `glib-2.0` newer than what `Debian Buster` provides. That's fine because we just need the `pipewire` libraries. Just edit `meson_options.txt` and remove `'wireplumber'` from the `session-managers` option.

## Provision your build directory

A couple build steps assume that you'll be building from the `/build` directory. This can either be a physical directory or a symlink to elsewhere. This is currently a hard-requirement, but may not be in the future. Sorry.

- `cd /build`
- `git clone https://github.com/clangen/musikcube.git --recursive`

## Compile third-party dependencies

The vendor script builds third-party dependencies from source with only the features musikcube needs. FFmpeg links Ogg, Vorbis, and Opus statically.

- `cd /build`
- `node ./musikcube/script/build-vendor-libraries.js`

Choose a compiler when prompted, or pass `--compiler default` to use the native compiler without a prompt.

Pass `--verbose` to stream build command output. By default, the script shows progress and prints recent command output on failure.

This process may take a while, but when it completes you'll be left with a `/build/vendor-${arch}` directory that will contain all build artifacts. The `musikcube` app compile will reference this directory during its build process.

Before publishing the vendor directory, the builder checks the shared libraries for unexpected dependencies using `readelf` on Linux and `otool` on macOS. To rerun the check on an existing build, use `node ./musikcube/script/scan-standalone.js --vendor ./vendor-<arch>/bin/lib`.

### Build the Linux x86-64 vendor libraries with Zig

From the build root, run `node ./musikcube/script/build-vendor-libraries.js --compiler zig`. This targets glibc 2.28 and publishes to `vendor-x86_64-zig-glibc2.28`. The application build still uses the regular vendor directory.

The builder stages the pinned Zig release automatically. To stage it separately, run `node ./musikcube/script/stage-zig-toolchain.js .` from the build root. The script prints the directory containing the compiler wrappers. The toolchain, downloads, caches, and temporary files live in `.vendor-cache` under that root.

## Compile `musikcube`

Use a C++20-capable compiler for the application build.

Now that we have all required dependencies available it's time to compile the app itself. As mentioned above we want the resulting binary to be relocatable on the filesystem, so some special pre- and post-processing must happen. Specifically: we need to gather copies of all dependencies and rewrite their `rpaths` if necessary. We also need to collect and include resource files like themes and localization files, then create `.deb`, `.rpm`, and `.tar.bz2` archives for distribution. As in the previous step all of this complexity has been hidden behind a script.

- `cd /build/musikcube`
- `./script/archive-standalone-nix.sh x.y.z`
  - `x.y.z` is the version of `musikcube` we'd like to appear in the filename. You can use anything you want, e.g. `0.0.0`
  - The build process will automatically discover and use the `/build/vendor-${arch}` directory created in the previous step.

After the script has completed you should have a set of "relocatable" binaries in `/build/musikcube/dist/x.y.z/` that will work on most Linux distributions released in 2018 or later.
