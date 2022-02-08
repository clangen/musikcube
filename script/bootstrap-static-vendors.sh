#!/bin/bash

rm -rf vendor
mkdir vendor
cd vendor

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"

#
# boost
#

wget https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.bz2
tar xvfj boost_1_78_0.tar.bz2
cd boost_1_78_0/
./bootstrap.sh
./b2 -d+2 cxxflags="-fPIC" --prefix=../boost-bin/ install
cd ..

#
# ffmpeg
#

wget https://ffmpeg.org/releases/ffmpeg-5.0.tar.bz2
tar xvfj ffmpeg-5.0.tar.bz2
cd ffmpeg-5.0
./configure \
    --pkg-config-flags="--static" \
    --prefix="../ffmpeg-bin/" \
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
    --enable-decoder=pcm_zork \
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
    --enable-encoder=wavpack \
    --enable-encoder=wmav1 \
    --enable-encoder=wmav2 \
    --disable-pthreads \
    --build-suffix=-musikcube || exit $?
make || exit $?
make install
cd ..

#
# libmp3lame
#

wget https://phoenixnap.dl.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
tar xvfz lame-3.100.tar.gz
cd lame-3.100/
./configure --enable-shared --enable-static --prefix=`pwd`/output
make
make install
mv output ../lame-bin
cd ..

#
# libmicrohttpd
#

wget https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.75.tar.gz
tar xvfz libmicrohttpd-0.9.75.tar.gz
cd libmicrohttpd-0.9.75
./configure --enable-shared --enable-static --with-pic --enable-https=no --disable-curl --prefix=`pwd`/output
make
make install
mv output ../libmicrohttpd-bin
cd ..

#
# libz
#

wget https://zlib.net/zlib-1.2.11.tar.gz
tar xvfz zlib-1.2.11.tar.gz
cd zlib-1.2.11
./configure --prefix=`pwd`/output
make
make install
mv output ../zlib-bin
cd ..

#
# curl
#

wget https://curl.se/download/curl-7.81.0.tar.gz
tar xvfz curl-7.81.0.tar.gz
cd curl-7.81.0
./configure \
    --enable-shared \
    --enable-static \
    --with-pic \
    --with-openssl \
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
    --without-brotli \
    --without-libidn2 \
    --prefix=`pwd`/output
make
make install
mv output ../curl-bin
cd ..