#!/bin/sh
# autogen.sh: Run this to set up the build system: configure, makefiles, etc.

# copyright by the mpg123 project - free software under the terms of the LGPL 2.1
# see COPYING and AUTHORS files in distribution or http://mpg123.org
# initially written by Nicholas J. Humfrey

package="mpg123"


srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"
DIE=0

(autoheader --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have autoconf installed to compile $package."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have autoconf installed to compile $package."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have automake installed to compile $package."
    echo "Download the appropriate package for your system,"
    echo "or get the source from one of the GNU ftp sites"
    echo "listed in http://www.gnu.org/order/ftp.html"
    DIE=1
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}


if test "$DIE" -eq 1; then
    exit 1
fi



echo "Generating configuration files for $package, please wait...."

# Create the (empty) build directory if it doesn't exist
if ! [ -d build ]; then
	echo "  creating build directory"
	mkdir build
fi



run_cmd() {
    echo "  running $* ..."
    if ! $*; then
			echo failed!
			exit 1
    fi
}


# Automake 1.10 borks up with LTCCASCOMPILE... defines LTCPPCOMPILE instead ... what is correct??
run_cmd libtoolize --force --copy --ltdl &&
perl -pi -e 's/AC_PROG_LIBTOOL/m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])
AC_PROG_LIBTOOL/' libltdl/configure.ac && cd libltdl &&
run_cmd aclocal-1.9 &&
run_cmd autoheader  &&
run_cmd automake-1.9 --add-missing --copy &&
run_cmd autoconf &&
cd .. || exit
run_cmd aclocal-1.9
run_cmd autoheader
run_cmd automake-1.9 --add-missing --copy
run_cmd autoconf

echo "Fixing that darned libltdl Makefile.in... it wants to blow on make dist because it's directory has been precreated!!!!"
echo "This may just apply to libtool-1.5.22 ... libtool-1.5.24 breaks in different ways, but also for make dist (mkinstalldirs target)"
perl -pi -e 's/mkdir \$\(distdir\)/\$(mkdir_p) \$(distdir)/' libltdl/Makefile.in

# Create mpg123.spec here... so that configure doesn't recreate it all the time.
NAME=`perl -ne    'if(/^AC_INIT\(\[([^,]*)\]/)         { print $1; exit; }' < configure.ac`
VERSION=`perl -ne 'if(/^AC_INIT\([^,]*,\s*\[([^,]*)\]/){ print $1; exit; }' < configure.ac`
perl -pe 's/\@PACKAGE_NAME\@/'$NAME'/; s/\@PACKAGE_VERSION\@/'$VERSION'/;' < mpg123.spec.in > mpg123.spec

$srcdir/configure --enable-debug && echo
