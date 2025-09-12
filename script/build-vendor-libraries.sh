#!/bin/bash

# this script is used to download and build all non-system-provided dependencies
# used by musikcube. they are configured with only the features that are strictly
# necessary for the app to function, and processed in stage in such a way that they
# have no external dependencies except for each other, libc, libc++ and libz.
#
# this script can also be configured to cross-compile the aforementioned dependencies
# for use with raspberry pi; simply set the CROSSCOMPILE=rpi to do so.
#
# the script will create a "vendor" subdirectory in the current path, and stage
# all final files in "vendor/lib".
#
# dependencies: openssl, curl, libmicrohttpd, ffmpeg, lame, libopenmpt

# set -x

#
# vars
#

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC -std=c++17"

RPATH="@rpath"

OS=$(uname)
ARCH=$(uname -m)
SCRIPTDIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
OPENSSL_VERSION="3.5.2"
CURL_VERSION="8.15.0"
LIBMICROHTTPD_VERSION="1.0.2"
FFMPEG_VERSION="8.0"
LAME_VERSION="3.100"
LIBOPENMPT_VERSION="0.8.2"
TAGLIB_VERSION="2.1.1"
GME_VERSION="0.6.3"
OUTDIR="$(pwd)/vendor/bin"
LIBDIR="$OUTDIR/lib"

JOBS="-j8"
if [[ $OS == "Darwin" ]]; then
    JOBS="-j$(sysctl -n hw.ncpu)"
elif nproc &> /dev/null; then
    JOBS="-j$(nproc)"
fi

OPENSSL_TYPE="linux-${ARCH}"
OPENSSL_EXTRA_LIBS=""
if [[ $OS == "Darwin" ]]; then
    OPENSSL_TYPE="darwin64-${ARCH}-cc"
fi

BUILD_ROOT="/build"

function set_makefile_env_vars() {
    if [[ -n $CROSSCOMPILE ]]; then
        printf "updating AR, CC, CXX, LD for Makefile cross-compile\n"
        OLD_AR=$AR
        OLD_CC=$CC
        OLD_CXX=$CXX
        OLD_LD=$LD
        export AR="${XTOOLS_TOOLCHAIN_NAME}-ar"
        export CC="${XTOOLS_TOOLCHAIN_NAME}-gcc"
        export CXX="${XTOOLS_TOOLCHAIN_NAME}-g++"
        export LD="${XTOOLS_TOOLCHAIN_NAME}-ld"
    fi
}

function unset_makefile_env_vars() {
    if [[ -n $CROSSCOMPILE ]]; then
        printf "restoring AR, CC, CXX, LDD for autotools/cmake cross-compile\n"
        AR=$OLD_AR
        CC=$OLD_CC
        CXX=$OLD_CXX
        LD=$OLD_LD
    fi
}

function configure_crosscompile_if_necessary() {
    # update cross-compile vars, if specified.
    if [[ $CROSSCOMPILE == rpi-* ]]; then
        # cross-compile toolchains found here: https://github.com/tttapa/docker-arm-cross-toolchain. note
        # that the toolchain uses a newer version of libstdc++ than supported by our oldest platform,
        # so we link with "-static-libstdc++".
        # see: https://github.com/tttapa/docker-arm-cross-toolchain/issues/5#issuecomment-1665744288

        OPENSSL_TYPE="linux-generic32"

        XTOOLS_TOOLCHAIN_NAME="armv8-rpi3-linux-gnueabihf"
        CMAKE_COMPILER_TOOLCHAIN="--toolchain ${BUILD_ROOT}/musikcube/.cmake/RaspberryPiToolchain-armv8.cmake"

        if [[ $CROSSCOMPILE == "rpi-armv6" ]]; then
            XTOOLS_TOOLCHAIN_NAME="armv6-rpi-linux-gnueabihf"
            CMAKE_COMPILER_TOOLCHAIN="--toolchain ${BUILD_ROOT}/musikcube/.cmake/RaspberryPiToolchain-armv6.cmake"
        fi

        XTOOLS_SYSROOT="${BUILD_ROOT}/x-tools/${XTOOLS_TOOLCHAIN_NAME}/${XTOOLS_TOOLCHAIN_NAME}/sysroot/"
        XTOOLS_LDFLAGS="--sysroot=${XTOOLS_SYSROOT}"
        XTOOLS_BIN_PATH="${BUILD_ROOT}/x-tools/${XTOOLS_TOOLCHAIN_NAME}/bin"
        XTOOLS_PKG_CONFIG_PATH="${XTOOLS_SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig/"
        VENDOR_PKG_CONFIG_PATH="${LIBDIR}/pkgconfig/"

        # in general these are all we need for autotools
        export PATH="${XTOOLS_BIN_PATH}:$PATH"
        export LDFLAGS="$LDFLAGS ${XTOOLS_LDFLAGS}"
        export CXXFLAGS="$CXXFLAGS -static-libstdc++"
        export PKG_CONFIG_PATH="${VENDOR_PKG_CONFIG_PATH}:${XTOOLS_PKG_CONFIG_PATH}"

        OPENSSL_CROSSCOMPILE_PREFIX="--cross-compile-prefix=${XTOOLS_TOOLCHAIN_NAME}-"
        OPENSSL_EXTRA_LIBS="-latomic"
        GENERIC_CONFIGURE_FLAGS="--build=x86_64-pc-linux-gnu --host=${XTOOLS_TOOLCHAIN_NAME} --with-sysroot=${XTOOLS_SYSROOT}"
        FFMPEG_CONFIGURE_FLAGS="--arch=${ARCH} --target-os=linux --enable-cross-compile --sysroot=${XTOOLS_SYSROOT} --cross-prefix=${XTOOLS_TOOLCHAIN_NAME}-"
    fi

    if [[ $CROSSCOMPILE == "x86" ]]; then
        OPENSSL_TYPE="linux-x86"
        CFLAGS="$CFLAGS -I/usr/include/i386-linux-gnu/"
        CXXFLAGS="$CXXFLAGS -I/usr/include/i386-linux-gnu/"

        # X86_LOCAL_SYSROOT="${BUILD_ROOT}/x86-sysroot"
        # X86_GLOBAL_SYSROOT="/usr/i686-linux-gnu/"
        # OPENSSL_TYPE="linux-x86"
        # VENDOR_PKG_CONFIG_PATH="${LIBDIR}/pkgconfig/"
        # SYSROOT_PKG_CONFIG_PATH="${X86_LOCAL_SYSROOT}/usr/lib/i386-linux-gnu/pkgconfig"
        # CFLAGS="$CFLAGS -m32 -I${X86_GLOBAL_SYSROOT}/include/ --sysroot=${X86_LOCAL_SYSROOT}"
        # CXXFLAGS="$CXXFLAGS -m32 -I${X86_GLOBAL_SYSROOT}/include/ --sysroot=${X86_LOCAL_SYSROOT}"
        # LDFLAGS="$LDFLAGS -m32 -L${X86_GLOBAL_SYSROOT}/lib/ --sysroot=${X86_LOCAL_SYSROOT}"
        # GENERIC_CONFIGURE_FLAGS="--build=x86_64-pc-linux-gnu --host=x86"
        # export PKG_CONFIG_PATH="${VENDOR_PKG_CONFIG_PATH}:${SYSROOT_PKG_CONFIG_PATH}"
    fi
}

function print_build_configuration() {
    show_banner "build configuration"
    printf "  - XTOOLS_TOOLCHAIN_NAME=${XTOOLS_TOOLCHAIN_NAME}\n"
    printf "  - CMAKE_COMPILER_TOOLCHAIN=${CMAKE_COMPILER_TOOLCHAIN}\n"
    printf "  - CFLAGS=${CFLAGS}\n"
    printf "  - CXXFLAGS=${CXXFLAGS}\n"
    printf "  - LDFLAGS=${LDFLAGS}\n"
    printf "  - GENERIC_CONFIGURE_FLAGS=${GENERIC_CONFIGURE_FLAGS}\n"
    printf "  - OPENSSL_TYPE=${OPENSSL_TYPE}\n"
    printf "  - OPENSSL_EXTRA_LIBS=${OPENSSL_EXTRA_LIBS}\n"
    printf "  - OPENSSL_CROSSCOMPILE_PREFIX=${OPENSSL_CROSSCOMPILE_PREFIX}\n"
    printf "  - FFMPEG_CONFIGURE_FLAGS=${FFMPEG_CONFIGURE_FLAGS}\n"
    printf "  - PKG_CONFIG_PATH=${PKG_CONFIG_PATH}\n"
    sleep 3
}

#
# download deps
#

function copy_or_download {
    url_path=$1
    fn=$2
    wget_cache="/tmp/musikcube_build_wget_cache"
    mkdir -p wget_cache 2> /dev/null
    if [[ -f "$wget_cache/$fn" ]]; then
        cp "$wget_cache/$fn" .
    else
        wget -P $wget_cache "$url_path/$fn" || exit $?
        cp "$wget_cache/$fn" .  || exit $?
    fi
}

function fetch_packages() {
    # no trailing slash on url dirs!
    copy_or_download https://www.openssl.org/source openssl-${OPENSSL_VERSION}.tar.gz
    copy_or_download https://curl.se/download curl-${CURL_VERSION}.tar.gz
    copy_or_download https://ftp.gnu.org/gnu/libmicrohttpd libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz
    copy_or_download https://ffmpeg.org/releases ffmpeg-${FFMPEG_VERSION}.tar.bz2
    copy_or_download https://downloads.sourceforge.net/project/lame/lame/${LAME_VERSION} lame-${LAME_VERSION}.tar.gz
    copy_or_download https://lib.openmpt.org/files/libopenmpt/src libopenmpt-${LIBOPENMPT_VERSION}+release.makefile.tar.gz
    copy_or_download https://lib.openmpt.org/files/libopenmpt/src libopenmpt-${LIBOPENMPT_VERSION}+release.autotools.tar.gz
    copy_or_download https://github.com/taglib/taglib/releases/download/v${TAGLIB_VERSION} taglib-${TAGLIB_VERSION}.tar.gz
    copy_or_download https://bitbucket.org/mpyne/game-music-emu/downloads game-music-emu-${GME_VERSION}.tar.gz
}

function show_banner {
    printf "\n\n********************************************************************************\n"
    printf "*\n"
    printf "* $1\n"
    printf "*\n"
    printf "********************************************************************************\n\n"
}

#
# openssl
#

function build_openssl() {
    show_banner "building openssl..."
    tar xvfz openssl-${OPENSSL_VERSION}.tar.gz > /dev/null
    cd openssl-${OPENSSL_VERSION}
    perl ./Configure --prefix=${OUTDIR} no-ssl3 no-ssl3-method no-zlib ${OPENSSL_EXTRA_LIBS} ${OPENSSL_TYPE} ${OPENSSL_CROSSCOMPILE_PREFIX} || exit $?
    make -j8
    make install_sw
    cd ..
    # for some reason on Linux the libraries are installed to `../bin/lib64` instead of `../bin/lib` like
    # all other libraries... move things into `lib` as appropriate.
    if [ -d "bin/lib64" ]; then
        mkdir bin/lib/
        mv bin/lib64/pkgconfig/* bin/lib/pkgconfig/
        mv bin/lib64/* bin/lib/
        rm -rf bin/lib64
        perl -i.bak -0pe "s|lib64|lib|" bin/lib/pkgconfig/libcrypto.pc
        perl -i.bak -0pe "s|lib64|lib|" bin/lib/pkgconfig/libssl.pc
        perl -i.bak -0pe "s|lib64|lib|" bin/lib/pkgconfig/openssl.pc
    fi
}

#
# curl
#

function build_curl() {
    show_banner "building libcurl..."
    rm -rf curl-${CURL_VERSION}
    tar xvfz curl-${CURL_VERSION}.tar.gz > /dev/null
    cd curl-${CURL_VERSION}
    ./configure --enable-shared \
        --with-pic \
        --with-openssl="${OUTDIR}" \
        --enable-optimize \
        --enable-http \
        --enable-proxy \
        --enable-ipv6 \
        --disable-rtsp \
        --disable-ftp \
        --disable-ftps \
        --disable-gopher \
        --disable-gophers \
        --disable-pop3 \
        --disable-pop3s \
        --disable-smb \
        --disable-smbs \
        --disable-smtp \
        --disable-telnet \
        --disable-tftp \
        --disable-hsts \
        --disable-imap \
        --disable-mqtt \
        --disable-dict \
        --disable-ldap \
        --without-librtmp \
        --without-zstd \
        --without-brotli \
        --without-libidn2 \
        --without-nghttp2 \
        --without-ngtcp2 \
        --without-nghttp3 \
        --without-quiche \
        --without-msh3 \
        --without-libpsl \
         ${GENERIC_CONFIGURE_FLAGS} \
        --prefix=${OUTDIR} || exit $?
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# libmicrohttpd
#

function build_libmicrohttpd() {
    show_banner "building libmicrohttpd..."
    rm -rf libmicrohttpd-${LIBMICROHTTPD_VERSION}
    tar xvfz libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz > /dev/null
    cd libmicrohttpd-${LIBMICROHTTPD_VERSION}
    ./configure --enable-shared --with-pic --enable-https=no --disable-curl --prefix=${OUTDIR} ${GENERIC_CONFIGURE_FLAGS}
    make -j8 || exit $?
    make install
    cd ..
}

#
# ffmpeg
#

function build_ffmpeg() {
    show_banner "building ffmpeg..."
    # fix for cross-compile: https://github.com/NixOS/nixpkgs/pull/76915/files
    rm -rf ffmpeg-${FFMPEG_VERSION}
    tar xvfj ffmpeg-${FFMPEG_VERSION}.tar.bz2 > /dev/null

    OLD_LDFLAGS=$LDFLAGS
    OLD_CFLAGS=$CFLAGS
    export LDFLAGS="$LDFLAGS -lm"
    export CFLAGS="$CFLAGS -I${XTOOLS_SYSROOT}/usr/include/opus"

    cd ffmpeg-${FFMPEG_VERSION}
    ./configure \
        --prefix=${OUTDIR} \
        --pkg-config="pkg-config" \
        --enable-rpath \
        --disable-asm \
        --enable-pic \
        --enable-shared \
        --disable-everything \
        --disable-programs \
        --disable-doc \
        --disable-debug \
        --disable-dxva2 \
        --disable-avdevice \
        --disable-avfilter \
        --disable-swscale \
        --disable-ffplay \
        --disable-network \
        --disable-muxers \
        --disable-demuxers \
        --disable-zlib \
        --disable-bzlib \
        --disable-iconv \
        --disable-bsfs \
        --disable-filters \
        --disable-parsers \
        --disable-indevs \
        --disable-outdevs \
        --disable-encoders \
        --disable-decoders \
        --disable-hwaccels \
        --disable-nvenc \
        --disable-videotoolbox \
        --disable-audiotoolbox \
        --disable-filters \
        --disable-libxcb \
        --disable-libxcb-shm \
        --disable-libxcb-xfixes \
        --disable-libxcb-shape \
        --disable-sdl2 \
        --disable-securetransport \
        --disable-vaapi \
        --disable-xlib \
        --enable-libopus \
        --enable-libvorbis \
        --enable-demuxer=aac \
        --enable-demuxer=ac3 \
        --enable-demuxer=aiff \
        --enable-demuxer=ape \
        --enable-demuxer=asf \
        --enable-demuxer=au \
        --enable-demuxer=avi \
        --enable-demuxer=flac \
        --enable-demuxer=flv \
        --enable-demuxer=matroska \
        --enable-demuxer=m4v \
        --enable-demuxer=mp3 \
        --enable-demuxer=mpc \
        --enable-demuxer=mpc8 \
        --enable-demuxer=ogg \
        --enable-demuxer=mov \
        --enable-demuxer=pcm_alaw \
        --enable-demuxer=pcm_mulaw \
        --enable-demuxer=pcm_f64be \
        --enable-demuxer=pcm_f64le \
        --enable-demuxer=pcm_f32be \
        --enable-demuxer=pcm_f32le \
        --enable-demuxer=pcm_s32be \
        --enable-demuxer=pcm_s32le \
        --enable-demuxer=pcm_s24be \
        --enable-demuxer=pcm_s24le \
        --enable-demuxer=pcm_s16be \
        --enable-demuxer=pcm_s16le \
        --enable-demuxer=pcm_s8 \
        --enable-demuxer=pcm_u32be \
        --enable-demuxer=pcm_u32le \
        --enable-demuxer=pcm_u24be \
        --enable-demuxer=pcm_u24le \
        --enable-demuxer=pcm_u16be \
        --enable-demuxer=pcm_u16le \
        --enable-demuxer=pcm_u8 \
        --enable-demuxer=wav \
        --enable-demuxer=wv \
        --enable-demuxer=xwma \
        --enable-demuxer=dsf \
        --enable-decoder=aac \
        --enable-decoder=aac_latm \
        --enable-decoder=ac3 \
        --enable-decoder=alac \
        --enable-decoder=als \
        --enable-decoder=ape \
        --enable-decoder=atrac1 \
        --enable-decoder=atrac3 \
        --enable-decoder=eac3 \
        --enable-decoder=flac \
        --enable-decoder=mp1 \
        --enable-decoder=mp1float \
        --enable-decoder=mp2 \
        --enable-decoder=mp2float \
        --enable-decoder=mp3 \
        --enable-decoder=mp3adu \
        --enable-decoder=mp3adufloat \
        --enable-decoder=mp3float \
        --enable-decoder=mp3on4 \
        --enable-decoder=mp3on4float \
        --enable-decoder=mpc7 \
        --enable-decoder=mpc8 \
        --enable-decoder=opus \
        --enable-decoder=vorbis \
        --enable-decoder=wavpack \
        --enable-decoder=wmalossless \
        --enable-decoder=wmapro \
        --enable-decoder=wmav1 \
        --enable-decoder=wmav2 \
        --enable-decoder=wmavoice \
        --enable-decoder=pcm_alaw \
        --enable-decoder=pcm_bluray \
        --enable-decoder=pcm_dvd \
        --enable-decoder=pcm_f32be \
        --enable-decoder=pcm_f32le \
        --enable-decoder=pcm_f64be \
        --enable-decoder=pcm_f64le \
        --enable-decoder=pcm_lxf \
        --enable-decoder=pcm_mulaw \
        --enable-decoder=pcm_s8 \
        --enable-decoder=pcm_s8_planar \
        --enable-decoder=pcm_s16be \
        --enable-decoder=pcm_s16be_planar \
        --enable-decoder=pcm_s16le \
        --enable-decoder=pcm_s16le_planar \
        --enable-decoder=pcm_s24be \
        --enable-decoder=pcm_s24daud \
        --enable-decoder=pcm_s24le \
        --enable-decoder=pcm_s24le_planar \
        --enable-decoder=pcm_s32be \
        --enable-decoder=pcm_s32le \
        --enable-decoder=pcm_s32le_planar \
        --enable-decoder=pcm_u8 \
        --enable-decoder=pcm_u16be \
        --enable-decoder=pcm_u16le \
        --enable-decoder=pcm_u24be \
        --enable-decoder=pcm_u24le \
        --enable-decoder=pcm_u32be \
        --enable-decoder=pcm_u32le \
        --enable-decoder=dsd_lsbf \
        --enable-decoder=dsd_msbf \
        --enable-decoder=dsd_lsbf_planar \
        --enable-decoder=dsd_msbf_planar \
        --enable-parser=aac \
        --enable-parser=aac_latm \
        --enable-parser=ac3 \
        --enable-parser=cook \
        --enable-parser=dca \
        --enable-parser=flac \
        --enable-parser=mpegaudio \
        --enable-parser=opus \
        --enable-parser=vorbis \
        --enable-muxer=adts \
        --enable-muxer=flac \
        --enable-muxer=ogg \
        --enable-muxer=opus \
        --enable-muxer=webm \
        --enable-muxer=webp \
        --enable-muxer=mov \
        --enable-muxer=mp4 \
        --enable-encoder=aac \
        --enable-encoder=alac \
        --enable-encoder=flac \
        --enable-encoder=mpeg4 \
        --enable-encoder=libopus \
        --enable-encoder=wavpack \
        --enable-encoder=wmav1 \
        --enable-encoder=wmav2 \
        --enable-encoder=libvorbis \
         ${FFMPEG_CONFIGURE_FLAGS} \
        --build-suffix=-musikcube || exit $?
    make ${JOBS} || exit $?
    make install

    export LDFLAGS=$OLD_LDFLAGS
    export CFLAGS=$OLD_CFLAGS

    cd ..
}

#
# lame
#

function build_lame() {
    show_banner "building lame..."
    rm -rf lame-${LAME_VERSION}
    tar xvfz lame-${LAME_VERSION}.tar.gz > /dev/null
    cd lame-${LAME_VERSION}
    # https://sourceforge.net/p/lame/mailman/message/36081038/
    perl -i.bak -0pe "s|lame_init_old\n||" include/libmp3lame.sym
    ./configure --disable-dependency-tracking --disable-debug --enable-nasm --prefix=${OUTDIR} ${GENERIC_CONFIGURE_FLAGS} || exit $?
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# libopenmpt
#

function build_libopenmpt() {
    show_banner "building libopenmpt..."
    # macOS needs to use the autotools version, but Linux uses the Makefile
    # version for cross-compile support.
    if [[ $OS == "Darwin" ]]; then
        rm -rf libopenmpt-${LIBOPENMPT_VERSION}+release.autotools
        tar xvfz libopenmpt-${LIBOPENMPT_VERSION}+release.autotools.tar.gz > /dev/null
        cd libopenmpt-${LIBOPENMPT_VERSION}+release.autotools
        ./configure \
            --disable-dependency-tracking \
            --enable-shared \
            --disable-openmpt123 \
            --disable-examples \
            --disable-tests \
            --disable-doxygen-doc \
            --disable-doxygen-html \
            --without-mpg123 \
            --without-ogg \
            --without-vorbis \
            --without-vorbisfile \
            --without-portaudio \
            --without-portaudiocpp \
            --without-sndfile \
            --without-flac \
            ${GENERIC_CONFIGURE_FLAGS} \
            --prefix=${OUTDIR} || exit $?
        make ${JOBS} || exit $?
        make install
        cd ..
    else
        set_makefile_env_vars
        rm -rf libopenmpt-0.7.0+release/
        tar xvfz libopenmpt-${LIBOPENMPT_VERSION}+release.makefile.tar.gz > /dev/null
        cd libopenmpt-${LIBOPENMPT_VERSION}+release
        OPENMPT_OPTIONS="EXAMPLES=0 NO_FLAC=1 NO_MINIMP3=1 NO_MINIZ=1 NO_MPG123=1 NO_OGG=1 NO_PORTAUDIO=1 NO_PORTAUDIOCPP=1 NO_PULSEAUDIO=1 NO_SDL2=1 NO_SNDFILE=1 NO_STBVORBIS=1 NO_VORBIS=1 NO_VORBISFILE=1 OPENMPT123=0 SHARED_LIB=1 STATIC_LIB=0 TEST=0 PREFIX=${OUTDIR}"
        make ${OPENMPT_OPTIONS} ${JOBS} VERBOSE=1 || exit $?
        make ${OPENMPT_OPTIONS} install
        unset_makefile_env_vars
        cd ..
    fi
}

#
# gme
#

function build_gme() {
    show_banner "building gme (game-music-emu)..."
    rm -rf game-music-emu-${GME_VERSION}
    tar xvfz game-music-emu-${GME_VERSION}.tar.gz > /dev/null
    cd game-music-emu-${GME_VERSION}
    cmake \
        ${CMAKE_COMPILER_TOOLCHAIN} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_INSTALL_PREFIX=${OUTDIR} \
        -DENABLE_UBSAN=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_SHARED_LIBS=1 \
        . || exit $?
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# taglib
#

function build_taglib() {
    show_banner "building taglib..."
    rm -rf taglib-${TAGLIB_VERSION}
    tar xvfz taglib-${TAGLIB_VERSION}.tar.gz > /dev/null
    cd taglib-${TAGLIB_VERSION}
    cmake \
        ${CMAKE_COMPILER_TOOLCHAIN} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_INSTALL_PREFIX=${OUTDIR} \
        -DBUILD_SHARED_LIBS=1 \
        . || exit $?
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# macOS dylib rpaths
#

function stage_prebuilt_libraries() {
    if [[ $OS == "Darwin" ]]; then
        BREW=$(brew --prefix)
        LIBOPUS="$BREW/opt/opus/lib/libopus.0.dylib"
        LIBOGG="$BREW/opt/libogg/lib/libogg.0.dylib"
        LIBVORBIS="$BREW/opt/libvorbis/lib/libvorbis.0.dylib"
        LIBVORBISENC="$BREW/opt/libvorbis/lib/libvorbisenc.2.dylib"
        mkdir -p bin/lib/
        cp ${LIBOPUS} ${LIBOGG} ${LIBVORBIS} ${LIBVORBISENC} bin/lib/
        chmod 755 bin/lib/*.dylib
    fi
}

function relink_dynamic_libraries() {
    node ${SCRIPTDIR}/relink-dynamic-libraries.js bin/lib
}

function delete_unused_libraries() {
    cd bin/lib/
    rm *.a 2> /dev/null
    rm *.la 2> /dev/null
    if [[ $OS == "Darwin" ]]; then
      mv libavcodec-musikcube.62.11.100.dylib libavcodec-musikcube.62.dylib
      rm libavcodec-musikcube.dylib
      ln -s libavcodec-musikcube.62.dylib libavcodec-musikcube.dylib

      mv libavformat-musikcube.62.3.100.dylib libavformat-musikcube.62.dylib
      rm libavformat-musikcube.dylib
      ln -s libavformat-musikcube.62.dylib libavformat-musikcube.dylib

      mv libavutil-musikcube.60.8.100.dylib libavutil-musikcube.60.dylib
      rm libavutil-musikcube.dylib
      ln -s libavutil-musikcube.60.dylib libavutil-musikcube.dylib

      mv libswresample-musikcube.6.1.100.dylib libswresample-musikcube.6.dylib
      rm libswresample-musikcube.dylib
      ln -s libswresample-musikcube.6.dylib libswresample-musikcube.dylib
    fi
    cd ../../
}

function main() {
    rm -rf vendor
    mkdir vendor
    cd vendor
    touch ".config-${ARCH}-${CROSSCOMPILE}"

    configure_crosscompile_if_necessary
    print_build_configuration

    stage_prebuilt_libraries
    fetch_packages

    build_ffmpeg
    build_openssl # must come before curl
    build_curl
    build_libopenmpt
    build_libmicrohttpd
    build_lame
    build_taglib
    build_gme

    delete_unused_libraries
    relink_dynamic_libraries

    cd ..

    if [[ $CROSSCOMPILE == rpi-* ]]; then
        mv vendor vendor-${CROSSCOMPILE}
    else
        mv vendor vendor-$(uname -m)
    fi
}

main

printf "\n\ndone!\n\n"
