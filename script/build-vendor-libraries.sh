#!/bin/bash

set -x

#
# vars
#

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"

OS=$(uname)
ARCH=$(uname -m)
BOOST_VERSION="1_76_0"
OPENSSL_VERSION="1.1.1m"
OPENSSL_LIB_PATH="$(pwd)/openssl-${OPENSSL_VERSION}/output/lib"
CURL_VERSION="7.81.0"
LIBMICROHTTPD_VERSION="0.9.75"
FFMPEG_VERSION="5.0"
LAME_VERSION="3.100"

function clean() {
    rm -rf vendor
    mkdir vendor
}

#
# download deps
#

function fetch_packages() {
    wget https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_${BOOST_VERSION}.tar.bz2
    wget https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz
    wget https://curl.se/download/curl-${CURL_VERSION}.tar.gz
    wget https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz
    wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2
    wget https://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz
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
    ./bootstrap.sh
    ./b2 headers
    ./b2 -d -j8 -sNO_LZMA=1 -sNO_ZSTD=1 threading=multi link=shared,static cxxflags=${BOOST_CXX_FLAGS} --prefix=../boost-bin/ install || exit $?
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

    rm -rf openssl-bin
    tar xvfz openssl-${OPENSSL_VERSION}.tar.gz
    cd openssl-${OPENSSL_VERSION}
    perl ./Configure --prefix=`pwd`/output no-ssl3 no-ssl3-method no-zlib ${OPENSSL_TYPE}
    make
    make install
    mkdir ../openssl-bin
    cp -rfp output/* ../openssl-bin
    cd ..

    if [ $OS == "Darwin" ]; then
        cd openssl-bin/lib
        install_name_tool -id "@rpath/libcrypto.1.1.dylib" libcrypto.1.1.dylib
        install_name_tool -id "@rpath/libcrypto.dylib" libcrypto.dylib
        install_name_tool -id "@rpath/libssl.1.1.dylib" libssl.1.1.dylib
        install_name_tool -id "@rpath/libssl.dylib" libssl.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libcrypto.1.1.dylib" "@rpath/libcrypto.1.1.dylib" libssl.1.1.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libcrypto.1.1.dylib" "@rpath/libcrypto.dylib" libssl.dylib
        cd ../../
    fi
}

#
# curl
#

function build_curl() {
    rm -rf curl-${CURL_VERSION}
    rm -rf curl-bin
    tar xvfz curl-${CURL_VERSION}.tar.gz
    cd curl-${CURL_VERSION}
    ./configure --enable-shared \
        --enable-static \
        --with-pic \
        --with-openssl="../openssl-bin/" \
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
        --without-brotli \
        --without-libidn2 \
        --without-nghttp2 \
        --prefix=`pwd`/output/
    make -j8
    make install
    mkdir ../curl-bin
    cp -rfp output/* ../curl-bin
    cd ..

    if [ $OS == "Darwin" ]; then
        cd curl-bin/lib
        install_name_tool -id "@rpath/libcurl.dylib" libcurl.dylib
        install_name_tool -id "@rpath/libcurl.4.dylib" libcurl.4.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libcrypto.1.1.dylib" "@rpath/libcrypto.1.1.dylib" libcurl.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libcrypto.1.1.dylib" "@rpath/libcrypto.1.1.dylib" libcurl.4.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libssl.1.1.dylib" "@rpath/libssl.1.1.dylib" libcurl.dylib
        install_name_tool -change "${OPENSSL_LIB_PATH}/libssl.1.1.dylib" "@rpath/libssl.1.1.dylib" libcurl.4.dylib
        cd ../../
    fi
}

#
# libmicrohttpd
#

function build_libmicrohttpd() {
    rm -rf libmicrohttpd-${LIBMICROHTTPD_VERSION}
    rm -rf libmicrohttpd-bin
    tar xvfz libmicrohttpd-${LIBMICROHTTPD_VERSION}.tar.gz
    cd libmicrohttpd-${LIBMICROHTTPD_VERSION}
    ./configure --enable-shared --enable-static --with-pic --enable-https=no --disable-curl --prefix=`pwd`/output
    make -j8 || exit $?
    make install
    mkdir ../libmicrohttpd-bin
    cp -rfp output/* ../libmicrohttpd-bin/
    cd ..

    if [ $OS == "Darwin" ]; then
        cd libmicrohttpd-bin/lib/
        install_name_tool -id "@rpath/libmicrohttpd.dylib" libmicrohttpd.dylib
        install_name_tool -id "@rpath/libmicrohttpd.12.dylib" libmicrohttpd.12.dylib
        cd ../../
    fi
}

#
# ffmpeg
#

function stage_opus_ogg_vorbis() {
    if [ $OS == "Darwin" ]; then
        # instead of building opus, ogg and vorbis from source we snag them
        # from brew, update their dylib ids with @rpath, re-sign them, then create
        # new pkg-config files to point towards this directory. that way ffmpeg
        # will pick them up automatically.

        rm -rf opus-ogg-vorbis
        mkdir opus-ogg-vorbis
        cd opus-ogg-vorbis
        export PKG_CONFIG_PATH=$(pwd)

        # create pkg-config files to point towards this dir
        cp /opt/homebrew/opt/opus/lib/pkgconfig/opus.pc .
        cp /opt/homebrew/opt/libogg/lib/pkgconfig/ogg.pc .
        cp /opt/homebrew/opt/libvorbis/lib/pkgconfig/vorbis.pc .
        cp /opt/homebrew/opt/libvorbis/lib/pkgconfig/vorbisenc.pc .
        chmod 644 *.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" opus.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" ogg.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" vorbis.pc
        perl -i.bak -0pe "s|libdir.*\n|libdir=$(pwd)\n|" vorbisenc.pc
        rm *.bak

        # copy libs, update their ids, then resign
        LIBOPUS="/opt/homebrew/opt/opus/lib/libopus.0.dylib"
        LIBOGG="/opt/homebrew/opt/libogg/lib/libogg.0.dylib"
        LIBVORBIS="/opt/homebrew/opt/libvorbis/lib/libvorbis.0.dylib"
        LIBVORBISENC="/opt/homebrew/opt/libvorbis/lib/libvorbisenc.2.dylib"
        cp ${LIBOPUS} ${LIBOGG} ${LIBVORBIS} ${LIBVORBISENC} .
        chmod 755 *.dylib
        install_name_tool -id "@rpath/libopus.0.dylib" ./libopus.0.dylib
        install_name_tool -id "@rpath/libogg.0.dylib" ./libogg.0.dylib
        install_name_tool -id "@rpath/libvorbis.0.dylib" ./libvorbis.0.dylib
        install_name_tool -id "@rpath/libvorbisenc.2.dylib" ./libvorbisenc.2.dylib
        codesign --remove-signature ./libopus.0.dylib
        codesign --remove-signature ./libogg.0.dylib
        codesign --remove-signature ./libvorbis.0.dylib
        codesign --remove-signature ./libvorbisenc.2.dylib
        codesign --sign=- ./libopus.0.dylib
        codesign --sign=- ./libogg.0.dylib
        codesign --sign=- ./libvorbis.0.dylib
        codesign --sign=- ./libvorbisenc.2.dylib

        # these links are required for pkg-config to be happy
        ln -s libopus.0.dylib libopus.dylib
        ln -s libogg.0.dylib libogg.dylib
        ln -s libvorbis.0.dylib libvorbis.dylib
        ln -s libvorbisenc.2.dylib libvorbisenc.dylib

        cd ..
    fi
}

function build_ffmpeg() {
    rm -rf ffmpeg-${FFMPEG_VERSION} ffmpeg-bin
    mkdir -p ffmpeg-bin/lib

    tar xvfj ffmpeg-${FFMPEG_VERSION}.tar.bz2
    cd ffmpeg-${FFMPEG_VERSION}
    ./configure \
        --prefix="@rpath" \
        --enable-rpath \
        --disable-asm \
        --enable-pic \
        --enable-static \
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
        --build-suffix=-musikcube
    make -j8
    make install
    mkdir ../ffmpeg-bin
    cp -rfp \@rpath/* ../ffmpeg-bin
    cd ..

    if [ $OS == "Darwin" ]; then
        cd ffmpeg-bin/lib/
        install_name_tool -change "${LIBOPUS}" "@rpath/libopus.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBOPUS}" "@rpath/libopus.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBOGG}" "@rpath/libogg.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBOGG}" "@rpath/libogg.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBVORBIS}" "@rpath/libvorbis.0.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBVORBIS}" "@rpath/libvorbis.0.dylib" libavformat-musikcube.59.dylib
        install_name_tool -change "${LIBVORBISENC}" "@rpath/libvorbisenc.2.dylib" libavcodec-musikcube.59.dylib
        install_name_tool -change "${LIBVORBISENC}" "@rpath/libvorbisenc.2.dylib" libavformat-musikcube.59.dylib
        cd ../../
    fi
}

#
# lame
#

function build_lame() {
    rm -rf lame-${LAME_VERSION} lame-bin
    tar xvfz lame-${LAME_VERSION}.tar.gz
    cd lame-${LAME_VERSION}
    # https://sourceforge.net/p/lame/mailman/message/36081038/
    perl -i.bak -0pe "s|lame_init_old\n||" include/libmp3lame.sym
    ./configure --disable-dependency-tracking --disable-debug --enable-nasm --prefix=$(pwd)/output
    make -j8
    make install
    mkdir ../lame-bin
    cp -rfp output/* ../lame-bin
    cd ..

    if [ $OS == "Darwin" ]; then
        cd lame-bin/lib/
        install_name_tool -id "@rpath/libmp3lame.dylib" libmp3lame.dylib
        install_name_tool -id "@rpath/libmp3lame.0.dylib" libmp3lame.0.dylib
        cd ../../
    fi
}

cd vendor
clean
fetch_packages
build_boost
build_openssl
build_curl
build_libmicrohttpd
stage_opus_ogg_vorbis
build_ffmpeg
build_lame
