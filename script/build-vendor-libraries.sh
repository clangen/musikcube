#!/bin/bash

# set -x

#
# vars
#

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"

RPATH="@rpath"

OS=$(uname)
ARCH=$(uname -m)
BOOST_VERSION_URL_PATH="1.76.0"
BOOST_VERSION="1_76_0"
OPENSSL_VERSION="1.1.1m"
CURL_VERSION="7.81.0"
LIBMICROHTTPD_VERSION="0.9.75"
FFMPEG_VERSION="5.0"
LAME_VERSION="3.100"
LIBOPENMPT_VERSION="0.6.1"

OUTDIR="$(pwd)/vendor/bin"
LIBDIR="$OUTDIR/lib"

JOBS="-j8"
if [ $OS == "Darwin" ]; then
    JOBS="-j$(sysctl -n hw.ncpu)"
fi

# check cross-compile flags
CONFIGURE_FLAGS=
if [ $CROSSCOMPILE == "arm" ]; then
    ARM_ROOT="/build/rpi/sysroot/"
    export CFLAGS="$CFLAGS -I${ARM_ROOT}/usr/include"
    export CXXFLAGS="$CXXFLAGS -I${ARM_ROOT}/usr/include"
    export LDFLAGS="$LDFLAGS --sysroot=${ARM_ROOT} -L/${ARM_ROOT}/lib/arm-linux-gnueabihf/"
    CONFIGURE_FLAGS="--build=x86_64-pc-linux-gnu --host=arm-linux-gnueabihf --with-sysroot=${ARM_ROOT}"
    printf "\n\n\ndetected CROSSCOMPILE=${CROSSCOMPILE}\n"
    printf "  CFLAGS=${CFLAGS}\n  CXXFLAGS=${CXXFLAGS}\n  LDFLAGS=${LDFLAGS}\n  CONFIGURE_FLAGS=${CONFIGURE_FLAGS}\n\n\n"
    sleep 3
fi

function clean() {
    rm -rf vendor
    mkdir vendor
}

#
# download deps
#

function fetch_packages() {
    wget https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION_URL_PATH}/source/boost_${BOOST_VERSION}.tar.bz2
    wget https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz
    wget https://curl.se/download/curl-${CURL_VERSION}.tar.gz
    wget https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz
    wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2
    wget https://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz
    wget https://lib.openmpt.org/files/libopenmpt/src/libopenmpt-${LIBOPENMPT_VERSION}+release.autotools.tar.gz
}

#
# boost
#

function build_boost() {
    BOOST_CXX_FLAGS="-fPIC"
    if [ $OS == "Darwin" ]; then
        BOOST_CXX_FLAGS="-fPIC -std=c++14 -stdlib=libc++"
    fi

    tar xvfj boost_${BOOST_VERSION}.tar.bz2
    cd boost_${BOOST_VERSION}
    ./bootstrap.sh --with-libraries=atomic,chrono,date_time,filesystem,system,thread
    ./b2 headers
    ./b2 -d ${JOBS} -sNO_LZMA=1 -sNO_ZSTD=1 threading=multi link=shared cxxflags=${BOOST_CXX_FLAGS} --prefix=${OUTDIR} install || exit $?
    cd ..
}

#
# openssl
#

function build_openssl() {
    OPENSSL_TYPE="linux-${ARCH}"
    if [ $OS == "Darwin" ]; then
        OPENSSL_TYPE="darwin64-${ARCH}-cc"
    fi

    tar xvfz openssl-${OPENSSL_VERSION}.tar.gz
    cd openssl-${OPENSSL_VERSION}
    perl ./Configure --prefix=${OUTDIR} no-ssl3 no-ssl3-method no-zlib ${OPENSSL_TYPE} ${CONFIGURE_FLAGS}
    make
    make install
    cd ..
}

#
# curl
#

function build_curl() {
    rm -rf curl-${CURL_VERSION}
    tar xvfz curl-${CURL_VERSION}.tar.gz
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
         ${CONFIGURE_FLAGS} \
        --prefix=${OUTDIR}
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# libmicrohttpd
#

function build_libmicrohttpd() {
    rm -rf libmicrohttpd-${LIBMICROHTTPD_VERSION}
    tar xvfz libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz
    cd libmicrohttpd-${LIBMICROHTTPD_VERSION}
    ./configure --enable-shared --with-pic --enable-https=no --disable-curl --prefix=${OUTDIR} ${CONFIGURE_FLAGS}
    make -j8 || exit $?
    make install
    cd ..
}

#
# ffmpeg
#

function build_ffmpeg() {
    rm -rf ffmpeg-${FFMPEG_VERSION}
    tar xvfj ffmpeg-${FFMPEG_VERSION}.tar.bz2
    cd ffmpeg-${FFMPEG_VERSION}
    ./configure \
        --prefix=${OUTDIR} \
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
        --disable-xvmc \
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
         ${CONFIGURE_FLAGS} \
        --build-suffix=-musikcube
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# lame
#

function build_lame() {
    rm -rf lame-${LAME_VERSION}
    tar xvfz lame-${LAME_VERSION}.tar.gz
    cd lame-${LAME_VERSION}
    # https://sourceforge.net/p/lame/mailman/message/36081038/
    perl -i.bak -0pe "s|lame_init_old\n||" include/libmp3lame.sym
    ./configure --disable-dependency-tracking --disable-debug --enable-nasm --prefix=${OUTDIR} ${CONFIGURE_FLAGS}
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# libopenmpt
#

function build_libopenmpt() {
    rm -rf libopenmpt-${LIBOPENMPT_VERSION}+release.autotools
    tar xvfz libopenmpt-${LIBOPENMPT_VERSION}+release.autotools.tar.gz
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
         ${CONFIGURE_FLAGS} \
        --prefix=${OUTDIR}
    make ${JOBS} || exit $?
    make install
    cd ..
}

#
# macOS dylib rpaths
#

function stage_opus_ogg_vorbis() {
    if [ $OS == "Darwin" ]; then
        # instead of building opus, ogg and vorbis from source we snag them
        # from brew, update their dylib ids with @rpath, re-sign them, then create
        # new pkg-config files to point towards this directory. that way ffmpeg
        # will pick them up automatically.

        mkdir -p bin/lib/
        cd bin/lib/
        export PKG_CONFIG_PATH=$(pwd)

        BREW=$(brew --prefix)

        # create pkg-config files to point towards this dir
        cp $BREW/opt/opus/lib/pkgconfig/opus.pc .
        cp $BREW/opt/libogg/lib/pkgconfig/ogg.pc .
        cp $BREW/opt/libvorbis/lib/pkgconfig/vorbis.pc .
        cp $BREW/opt/libvorbis/lib/pkgconfig/vorbisenc.pc .
        chmod 644 *.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" opus.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" ogg.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" vorbis.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" vorbisenc.pc
        rm *.bak

        # copy libs, update their ids, then resign
        LIBOPUS="$BREW/opt/opus/lib/libopus.0.dylib"
        LIBOGG="$BREW/opt/libogg/lib/libogg.0.dylib"
        LIBVORBIS="$BREW/opt/libvorbis/lib/libvorbis.0.dylib"
        LIBVORBISENC="$BREW/opt/libvorbis/lib/libvorbisenc.2.dylib"

        cp ${LIBOPUS} ${LIBOGG} ${LIBVORBIS} ${LIBVORBISENC} .
        chmod 755 *.dylib

        install_name_tool -id "$RPATH/libopus.0.dylib" ./libopus.0.dylib
        codesign --remove-signature ./libopus.0.dylib
        codesign --sign=- ./libopus.0.dylib
        ln -s libopus.0.dylib libopus.dylib

        install_name_tool -id "$RPATH/libogg.0.dylib" ./libogg.0.dylib
        codesign --remove-signature ./libogg.0.dylib
        codesign --sign=- ./libogg.0.dylib
        ln -s libogg.0.dylib libogg.dylib

        install_name_tool -id "$RPATH/libvorbis.0.dylib" ./libvorbis.0.dylib
        install_name_tool -change "${LIBOGG}" "$RPATH/libogg.0.dylib" ./libvorbis.0.dylib
        ln -s libvorbis.0.dylib libvorbis.dylib

        install_name_tool -id "$RPATH/libvorbisenc.2.dylib" ./libvorbisenc.2.dylib
        install_name_tool -change "${LIBOGG}" "$RPATH/libogg.0.dylib" ./libvorbisenc.2.dylib
        install_name_tool -change "${LIBVORBIS}" "$RPATH/libvorbis.0.dylib" ./libvorbisenc.2.dylib
        # odd man out... not sure why this is this way...
        LIBVORBIS_CELLAR="$BREW/Cellar/libvorbis/1.3.7/lib/libvorbis.0.dylib"
        install_name_tool -change "${LIBVORBIS_CELLAR}" "$RPATH/libvorbis.0.dylib" ./libvorbisenc.2.dylib
        #end weird hack
        ln -s libvorbisenc.2.dylib libvorbisenc.dylib

        codesign --remove-signature ./libvorbis.0.dylib
        codesign --remove-signature ./libvorbisenc.2.dylib
        codesign --sign=- ./libvorbis.0.dylib
        codesign --sign=- ./libvorbisenc.2.dylib

        cd ../..
    fi
}

function patch_dylib_rpaths() {
    if [ $OS == "Darwin" ]; then
        cd bin/lib

        install_name_tool -id "$RPATH/libavutil-musikcube.57.dylib" libavutil-musikcube.57.dylib
        rm libavutil-musikcube.dylib
        ln -s libavutil-musikcube.57.dylib libavutil-musikcube.dylib

        # ffmpeg
        install_name_tool -id "$RPATH/libavformat-musikcube.59.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libswresample-musikcube.4.dylib" "$RPATH/libswresample-musikcube.4.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libavcodec-musikcube.59.dylib" "$RPATH/libavcodec-musikcube.59.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libavutil-musikcube.57.dylib" "$RPATH/libavutil-musikcube.57.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBOPUS}" "$RPATH/libopus.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBOGG}" "$RPATH/libogg.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBVORBIS}" "$RPATH/libvorbis.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBVORBISENC}" "$RPATH/libvorbisenc.2.dylib" libavformat-musikcube.59.dylib
        rm libavformat-musikcube.dylib
        ln -s libavformat-musikcube.59.dylib libavformat-musikcube.dylib

        install_name_tool -id "$RPATH/libavcodec-musikcube.59.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libswresample-musikcube.4.dylib" "$RPATH/libswresample-musikcube.4.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libavcodec-musikcube.59.dylib" "$RPATH/libavcodec-musikcube.59.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "$LIBDIR/libavutil-musikcube.57.dylib" "$RPATH/libavutil-musikcube.57.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBOPUS}" "$RPATH/libopus.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBOGG}" "$RPATH/libogg.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBVORBIS}" "$RPATH/libvorbis.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBVORBISENC}" "$RPATH/libvorbisenc.2.dylib" libavcodec-musikcube.59.dylib
        rm libavcodec-musikcube.dylib
        ln -s libavcodec-musikcube.59.dylib libavcodec-musikcube.dylib

        install_name_tool -id "$RPATH/libswresample-musikcube.4.dylib" libswresample-musikcube.4.dylib
        install_name_tool -change "$FFMPEG_LIB_PATH/libavutil-musikcube.57.dylib" "$RPATH/libavutil-musikcube.57.dylib" libswresample-musikcube.4.dylib
        rm libswresample-musikcube.dylib
        ln -s libswresample-musikcube.4.dylib libswresample-musikcube.dylib

        # openssl
        install_name_tool -id "$RPATH/libcrypto.1.1.dylib" libcrypto.1.1.dylib
        rm libcrypto.dylib
        ln -s libcrypto.1.1.dylib libcrypto.dylib

        install_name_tool -id "$RPATH/libssl.1.1.dylib" libssl.1.1.dylib
        install_name_tool -change "${LIBDIR}/libcrypto.1.1.dylib" "$RPATH/libcrypto.1.1.dylib" libssl.1.1.dylib
        rm libssl.dylib
        ln -s libssl.1.1.dylib libssl.dylib

        # curl
        install_name_tool -id "$RPATH/libcurl.4.dylib" libcurl.4.dylib
        install_name_tool -change "${LIBDIR}/libcrypto.1.1.dylib" "$RPATH/libcrypto.1.1.dylib" libcurl.4.dylib
        install_name_tool -change "${LIBDIR}/libssl.1.1.dylib" "$RPATH/libssl.1.1.dylib" libcurl.4.dylib
        rm libcurl.dylib
        ln -s libcurl.4.dylib libcurl.dylib

        # libmicrohttpd
        install_name_tool -id "$RPATH/libmicrohttpd.12.dylib" libmicrohttpd.12.dylib
        rm libmicrohttpd.dylib
        ln -s libmicrohttpd.12.dylib libmicrohttpd.dylib

        # lame
        install_name_tool -id "$RPATH/libmp3lame.0.dylib" libmp3lame.0.dylib
        rm libmp3lame.dylib
        ln -s libmp3lame.0.dylib libmp3lame.dylib

        # libopenmpt
        install_name_tool -id "$RPATH/libopenmpt.0.dylib" libopenmpt.0.dylib
        rm libopenmpt.dylib
        ln -s libopenmpt.0.dylib libopenmpt.dylib

        cd ../../
    fi
}

clean
mkdir vendor
cd vendor
fetch_packages
build_boost
build_openssl
build_curl
build_libmicrohttpd
stage_opus_ogg_vorbis
build_ffmpeg
build_lame
build_libopenmpt
patch_dylib_rpaths
