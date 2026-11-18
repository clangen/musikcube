#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cygpath -u "$1")
options_file=$(cygpath -u "$2")
prefix=$(cygpath -u "$3")
deps=$(cygpath -m "$4")
output_dir=$(cygpath -u "$5")

export PATH="$(cygpath -u "$6"):$PATH"
export PKG_CONFIG_LIBDIR="$(cygpath -u "$4")/lib/pkgconfig"
unset PKG_CONFIG_PATH CFLAGS CXXFLAGS LDFLAGS
export MSYS2_ARG_CONV_EXCL='*'
export CC=cl.exe

# MSVC cannot compile the empty CBS table produced by this audio-only profile.
# A skipped sentinel keeps the table valid without enabling extra codecs.
cbs="$source_dir/libavcodec/cbs.c"
if ! grep -q 'musikcube: empty-table sentinel' "$cbs"; then
    sed -i '/^static const CodedBitstreamType \*const cbs_type_table\[\] = {/a\    NULL, /* musikcube: empty-table sentinel */' "$cbs"
    sed -i 's/i = 0; i < FF_ARRAY_ELEMS(cbs_type_table)/i = 1; i < FF_ARRAY_ELEMS(cbs_type_table)/' "$cbs"
fi
grep -q 'musikcube: empty-table sentinel' "$cbs"
grep -q 'i = 1; i < FF_ARRAY_ELEMS(cbs_type_table)' "$cbs"

# Configure the shared FFmpeg profile for MSVC and the staged codec libraries.
mapfile -t options < <(tr -d '\r' < "$options_file")
cd "$output_dir"
"$source_dir/configure" "${options[@]}" \
    --toolchain=msvc --arch=x86_64 --enable-w32threads \
    --prefix="$prefix" --pkg-config-flags="--static --define-variable=prefix=$deps" \
    --extra-cflags="-MT -I$deps/include" --extra-ldflags="-LIBPATH:$deps/lib"

# FFmpeg canonicalizes the source directory to an MSYS path during configure.
# Use a relative path for the native compiler while keeping argument rewriting off.
source_relative=$(realpath --relative-to="$output_dir" "$source_dir")
sed -i "s|^SRC_PATH=.*|SRC_PATH=$source_relative|; s|^SRC_LINK=.*|SRC_LINK=$source_relative|" ffbuild/config.mak

make -j "${NUMBER_OF_PROCESSORS:-2}" all

install_relative=$(realpath --relative-to="$output_dir" "$prefix")
make install-libs install-headers prefix="./$install_relative"
