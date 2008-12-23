#!/bin/sh

# A dirty script to create some windows binaries (shared, static, debug, ...) using the MSYS environment.

temp="$PWD/tmp"
final="$PWD/releases"
txt="README COPYING NEWS"
opts="--with-audio=win32 --disable-modules"

# Get the version for the build from configure.ac .
version=`sed -n 's/^AC_INIT([^,]*, \[\([^,]*\)\], .*$/\1/p' < configure.ac`
echo "Building binaries for version $version"

prepare_dir()
{
	test -e "$final" || mkdir "$final"
}

prepare_unix2dos()
{
	echo "preparing unix2dos tool"
	# I'll include documentation in DOS-style, with the help of this little unix2dos variant.
	test -x "unix2dos" || echo "
#include <unistd.h>
#include <stdio.h>
int main()
{
	char buf[1000];
	ssize_t got;
	while((got=read(0, buf, 1000))>0)
	{
		ssize_t end=0;
		ssize_t pos=0;
		for(end=0;end<got;++end)
		{
			if(buf[end] == '\n')
			{
				write(1, buf+pos, end-pos);
				write(1, \"\r\n\", 2);
				pos=end+1;
			}
		}
		write(1, buf+pos, end-pos);
	}
}" > unix2dos.c && gcc -O -o unix2dos unix2dos.c
}

mpg123_build()
{
	cpu=$1
	stat=$2
	debug=$3
	myopts=$opts
	if test "$stat" = "y"; then
		echo "static build (stat=$stat)" &&
		name=mpg123-$version-static-$cpu
		myopts="$myopts --disable-shared"
	else
		echo "dynamic build (stat=$stat)" &&
		name=mpg123-$version-$cpu
	fi &&
	if test "$debug" = "y"; then
		echo "Debugging build!"
		name=$name-debug
		myopts="$myopts --enable-debug"
	fi &&
	tmp=$temp-$name &&
	echo "REMOVING $tmp!" &&
	sleep 5 &&
	if test -e Makefile; then make clean; fi &&
	rm -rvf $tmp &&
	./configure --prefix=$tmp $myopts --with-cpu=$cpu && make && make install &&
	rm -rf "$final/$name" &&
	mkdir  "$final/$name" &&
	cp -v "$tmp/bin/mpg123.exe" "$final/$name" &&
	if test "$debug" = y; then
		echo "Not stripping the debug build..."
	else
		strip --strip-unneeded "$final/$name/"*.exe
	fi &&
	if test "$stat" = "y"; then
		echo "No DLL there..."
	else
		cp -v "$tmp/bin/libmpg123"*.dll "$tmp/include/mpg123.h" "$final/$name" &&
		cp -v "src/libmpg123/.libs/libmpg123"*.dll.def "$final/$name" &&
		if test "$debug" = y; then
			echo "Not stripping the debug build..."
		else
			strip --strip-unneeded "$final/$name/"*.dll || exit 1
		fi
	fi &&
	for i in $txt
	do
		echo "text file $i -> $final/$name/$i.txt"
		./unix2dos < "$i" > "$final/$name/$i.txt"
	done
}

prepare_dir &&
prepare_unix2dos &&
mpg123_build x86 y n &&
mpg123_build x86_dither y n &&
mpg123_build x86 n n &&
mpg123_build x86_dither n n &&
mpg123_build x86 n y &&
echo "Hurray!" || echo "Bleh..."
