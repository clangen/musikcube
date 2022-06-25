#!/bin/bash

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
    unzip $fn
}

function stage_arch {
    arch=$1
    mkdir -p $arch/lib
    mkdir -p $arch/dll
    mkdir -p $arch/include
    mv stage/bin/
    mkdir lib
}

function process_x86() {
    mkdir x86
    cd x86
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x86 zlib-1.2.12-vs16-x86.zip
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x86 openssl-1.1.1n-vs16-x86.zip
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x86 libcurl-7.83.0-vs16-x86.zip
    copy_or_download https://lib.openmpt.org/files/libopenmpt/dev libopenmpt-0.6.3+release.dev.windows.vs2022.zip
    rm *.zip
    mkdir stage
    mv * stage/
    cd ..
}

function process_x64() {
    mkdir x64
    cd x64
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x64 zlib-1.2.12-vs16-x64.zip
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x64 openssl-1.1.1n-vs16-x64.zip
    copy_or_download https://windows.php.net/downloads/php-sdk/deps/vs16/x64 libcurl-7.83.0-vs16-x64.zip
    copy_or_download https://lib.openmpt.org/files/libopenmpt/dev libopenmpt-0.6.3+release.dev.windows.vs2022.zip
    rm *.zip
    mkdir stage
    mv * stage
    cd ..
}

rm -rf vendor
mkdir vendor
cd vendor
process_x86
process_x64
cd ..