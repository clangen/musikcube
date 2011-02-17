dnl configure.ac: source for the configure script

dnl copyright by the mpg123 project - free software under the terms of the LGPL 2.1
dnl see COPYING and AUTHORS files in distribution or http://mpg123.org
dnl initially written by Nicholas J. Humfrey

dnl Require autoconf version >= 2.57
AC_PREREQ(2.57)


dnl ############# Initialisation
AC_INIT([mpg123], [1.13.1], [mpg123-devel@lists.sourceforge.net])
dnl Increment API_VERSION when the API gets changes (new functions).
API_VERSION=29
LIB_PATCHLEVEL=1
dnl Since we want to be backwards compatible, both sides get set to API_VERSION.
LIBMPG123_VERSION=$API_VERSION:$LIB_PATCHLEVEL:$API_VERSION
AC_SUBST(LIBMPG123_VERSION)
AC_SUBST(API_VERSION)

AC_CONFIG_SRCDIR(src/mpg123.c)
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_SRCDIR(doc)
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

dnl Version 1.7 of automake is recommended
AM_INIT_AUTOMAKE(1.7)
AM_CONFIG_HEADER(src/config.h)


# You get strange symptoms like jack module build failing because the AC_C_CONST failed to detect the working const support.
# In that case, the test failed because -Werror, not because no const there...
# After looking again, there are possibly more tests being obscured by false failures.
AC_MSG_CHECKING([for -Werror in CFLAGS (It breaks tests)])
if echo "$CFLAGS" | grep Werror; then
	AC_MSG_RESULT([yes])
	AC_MSG_ERROR([I refuse to run with -Werror in CFLAGS. That breaks tests and makes this configure bogus.
If you want paranoid compilation, use --enable-nagging option, which adds -Werror for gcc.
Also note that you shall not run make distcheck after configuring with --enable-nagging.
distcheck uses the generated CFLAGS...])
else
	AC_MSG_RESULT([no])
fi

buffer=enabled # try to build with buffer by default

dnl ############# Compiler and tools Checks

LT_LDFLAGS=-export-dynamic
EXEC_LT_LDFLAGS=
be_static=no
all_static=no
AC_MSG_CHECKING([if you are up to something totally static with LDFLAGS/CFLAGS])
for f in $LDFLAGS $CFLAGS
do
	case "$f" in
		-all-static)
			be_static=yes
			all_static=yes
		;;
		-static)
			be_static=yes
		;;
	esac
done
if test "x$be_static" = xyes; then
	AC_MSG_RESULT([yes])
	LT_LDFLAGS=-all-static
	EXEC_LT_LDFLAGS="$LT_LDFLAGS"
else
	AC_MSG_RESULT([no])
fi
if test "x$all_static" = xyes; then
	AC_MSG_WARN( Use -static in LDFLAGS for all-static linking! Your compiler may blow up on that -all-static. )
fi

AM_PROG_AS
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
dnl "Checking for egrep is broken after removal of libltdl stuff... checks use $EGREP, so searching it here."
AC_PROG_EGREP
AC_C_CONST
AC_INLINE
AC_C_BIGENDIAN

dnl ############# Use Libtool for dynamic module loading

modules=auto
OUTPUT_OBJ="module.\$(OBJEXT)"
AC_ARG_ENABLE(modules,
[  --enable-modules=[no/yes] dynamically loadable output modules],
[
	if test "x$enableval" = xyes
	then
		modules=enabled
	else
		modules=disabled
	fi
],
[
	if test "x$be_static" = "xyes"; then
		modules=disabled
	else
		modules=auto
	fi
])

dnl We only want shared libraries by default
AC_DISABLE_STATIC
AC_ENABLE_SHARED

if test x"$enable_shared" = xno; then
	modules=disabled
	LT_LDFLAGS=
else
	AC_DEFINE(DYNAMIC_BUILD, 1, [ Define if building with dynamcally linked libmpg123])
fi

if test x"$modules" = xdisabled
then
	echo "Modules disabled, not checking for libltdl."
else
	have_ltdl=no
	AC_CHECK_LIB( [ltdl], [lt_dlinit],
		[ AC_CHECK_HEADER( [ltdl.h],
			[
				have_ltdl=yes
				AC_DEFINE(HAVE_LTDL, 1, [ Define if libltdl is available ])
			],
			[
				AC_MSG_WARN([Found ltdl library but NOT the header file on your system])
			] )
		],
		[
			AC_MSG_WARN([ltdl library not found])
		]
	)
	if test x"$modules" = xenabled -a x"$have_ltdl" = xno; then
		AC_MSG_ERROR([Modules enabled but no libltdl found! This will not work...])
	fi
	if test x"$modules" = xauto; then
		if test x"$have_ltdl" = xyes; then
			modules=enabled
			echo "We found libltdl: Modules enabled."
		else
			echo "We did not find libltdl: Modules disabled."
			modules=disabled
		fi
	fi
fi

if test x"$modules" = xdisabled
then
	echo "Modules disabled."
else
# Enable module support in source code
AC_DEFINE( USE_MODULES, 1, [Define if modules are enabled] )
fi

dnl Configure libtool

AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

if test x"$modules" = xdisabled
then
	echo "Modules still disabled..."
	MODULE_OBJ="legacy_module.\$(OBJEXT)"
else
	MODULE_OBJ="module.\$(OBJEXT)"
fi

AM_CONDITIONAL( [HAVE_MODULES], [test "x$modules" = xenabled] )

AC_SUBST(MODULE_OBJ)
AC_SUBST(LT_LDFLAGS)
AC_SUBST(EXEC_LT_LDFLAGS)

dnl ############## Configurable Options

AC_ARG_ENABLE(debug,
              [  --enable-debug=[no/yes] turn on debugging],
              [
                if test "x$enableval" = xyes
                then
                  debugging="enabled"
                else
                  debugging="disabled"
                fi
              ],
              [ debugging="disabled" ]
)

AC_ARG_ENABLE(nagging,
              [  --enable-nagging=[no/yes] turn on GCC's pedantic nagging with error on warnings, also enables debugging ],
              [
                if test "x$enableval" = xyes
                then
                  nagging="enabled"
                  debugging="enabled"
               else
                  nagging="disabled"
                fi
              ],
              [ nagging="disabled" ]
)

if test x"$debugging" = xenabled; then
	AC_DEFINE(DEBUG, 1, [ Define if debugging is enabled. ])
fi

AC_ARG_ENABLE(gapless,
              [  --enable-gapless=[no/yes] turn on gapless (enabled per default)],
              [
                if test "x$enableval" = xyes
                then
                  gapless="enabled"
                  AC_DEFINE(GAPLESS, 1, [ Define if gapless is enabled. ])
                else
                  gapless="disabled"
                fi
              ],
              [
                gapless="enabled"
                AC_DEFINE(GAPLESS, 1, [ Define if gapless is enabled. ])
              ]
)

AC_ARG_ENABLE(fifo,
              [  --enable-fifo=[no/yes] FIFO support for control interface (auto-enabled on linux) ],
              [
                if test "x$enableval" = xyes
                then
                  fifo="enabled"
                else
                  fifo="disabled"
                fi
              ],
              [
                fifo="auto"
              ]
)

AC_ARG_ENABLE(ipv6,
              [  --enable-ipv6=[no/yes] IPv6 support (actually any protocol your libc does with getaddrinfo) ],
              [
                if test "x$enableval" = xyes
                then
                  ipv6="enabled"
                else
                  ipv6="disabled"
                fi
              ],
              [
                ipv6="auto"
              ]
)

AC_ARG_ENABLE(network,
              [  --enable-network=[no/yes] network support (http streams / webradio) ],
              [
                if test "x$enableval" = xyes
                then
                  network="enabled"
                else
                  network="disabled"
                fi
              ],
              [
                network="auto"
              ]
)

dnl Optional objects list, depends on decoder choice and core feature selection.
dnl Not just for specific decoders anymore...
s_fpu=
DECODER_OBJ=
DECODER_LOBJ=

dnl Core features that can be disabled to reduce binary size.

id3v2=enabled
AC_ARG_ENABLE(id3v2,
              [  --disable-id3v2=[no/yes] no ID3v2 parsing ],
              [
                if test "x$enableval" = xno; then
                  id3v2="disabled"
                fi
              ], [])

# id3v2 depends on strings... so check that in between.
string=enabled
AC_ARG_ENABLE(string,
              [  --disable-string=[no/yes] no string API (this will disable ID3v2; main mpg123 won't build anymore) ],
              [
                if test "x$enableval" = xno; then
                  string="disabled"
                fi
              ], [])

if test "x$string" = "xdisabled"; then
  AC_DEFINE(NO_STRING, 1, [ Define to disable string functions. ])
  id3v2=disabled
 	AC_MSG_WARN([ID3v2 support disabled because of string API being disabled.])
else
  DECODER_OBJ="$DECODER_OBJ stringbuf.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ stringbuf.lo"
fi

if test "x$id3v2" = "xdisabled"; then
  AC_DEFINE(NO_ID3V2, 1, [ Define to disable ID3v2 parsing. ])
fi

icy=enabled
AC_ARG_ENABLE(icy,
              [  --disable-icy=[no/yes] no ICY metainfo parsing/conversion (main mpg123 won't build!) ],
              [
                if test "x$enableval" = xno; then
                  icy="disabled"
                fi
              ], [])

if test "x$icy" = "xdisabled"; then
  AC_DEFINE(NO_ICY, 1, [ Define to disable ICY handling. ])
else
  DECODER_OBJ="$DECODER_OBJ icy.\$(OBJEXT) icy2utf8.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ icy.lo icy2utf8.lo"
fi

ntom=enabled
AC_ARG_ENABLE(ntom,
              [  --disable-ntom=[no/yes] no flexible resampling ],
              [
                if test "x$enableval" = xno; then
                  ntom="disabled"
                fi
              ], [])

if test "x$ntom" = "xdisabled"; then
  AC_DEFINE(NO_NTOM, 1, [ Define to disable ntom resampling. ])
else
  DECODER_OBJ="$DECODER_OBJ ntom.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ ntom.lo"
fi

downsample=enabled
AC_ARG_ENABLE(downsample,
              [  --disable-downsample=[no/yes] no downsampled decoding ],
              [
                if test "x$enableval" = xno; then
                  downsample="disabled"
                fi
              ], [])

if test "x$downsample" = "xdisabled"; then
  AC_DEFINE(NO_DOWNSAMPLE, 1, [ Define to disable downsampled decoding. ])
fi

feeder=enabled
AC_ARG_ENABLE(feeder,
              [  --disable-feeder=[no/yes] no feeder decoding, no buffered readers ],
              [
                if test "x$enableval" = xno; then
                  feeder="disabled"
                fi
              ], [])

if test "x$feeder" = "xdisabled"; then
  AC_DEFINE(NO_FEEDER, 1, [ Define to disable feeder and buffered readers. ])
fi

messages=enabled
AC_ARG_ENABLE(messages,
              [  --disable-messages=[no/yes] no error/warning messages on the console ],
              [
                if test "x$enableval" = xno; then
                  messages="disabled"
                fi
              ], [])

if test "x$messages" = "xdisabled"; then
  AC_DEFINE(NO_WARNING, 1, [ Define to disable warning messages. ])
  AC_DEFINE(NO_ERROR,   1, [ Define to disable error messages. ])
  AC_DEFINE(NO_ERETURN, 1, [ Define to disable error messages in combination with a return value (the return is left intact). ])
fi

integers=fast
AC_ARG_ENABLE(int-quality,
[  --enable-int-quality=[yes/no] use rounding instead of fast truncation for integer output, where possible ],
[
  if test "x$enableval" = xyes; then
    integers=quality
    AC_DEFINE(ACCURATE_ROUNDING,   1, [ Define to use proper rounding. ])
  fi
], [])

int16=enabled
AC_ARG_ENABLE(16bit,
              [  --disable-16bit=[no/yes] no 16 bit integer output ],
              [
                if test "x$enableval" = xno; then
                  int16="disabled"
                fi
              ], [])

int8=enabled
AC_ARG_ENABLE(8bit,
              [  --disable-8bit=[no/yes] no 8 bit integer output ],
              [
                if test "x$enableval" = xno; then
                  int8="disabled"
                fi
              ], [])

int32=enabled
AC_ARG_ENABLE(32bit,
              [  --disable-32bit=[no/yes] no 32 bit integer output (also 24 bit) ],
              [
                if test "x$enableval" = xno; then
                  int32="disabled"
                fi
              ], [])

real=enabled
AC_ARG_ENABLE(real,
              [  --disable-real=[no/yes] no real (floating point) output ],
              [
                if test "x$enableval" = xno; then
                  real="disabled"
                fi
              ], [])

AC_ARG_WITH([cpu], [
  --with-cpu=generic[_fpu]      Use generic processor code with floating point arithmetic
  --with-cpu=generic_float      Plain alias to generic_fpu now... float output is a normal runtime option!
  --with-cpu=generic_nofpu      Use generic processor code with fixed point arithmetic (p.ex. ARM, experimental)
  --with-cpu=generic_dither     Use generic processor code with floating point arithmetic and dithering for 1to1 16bit decoding.
  --with-cpu=i386[_fpu]         Use code optimized for i386 processors with floating point arithmetic
  --with-cpu=i386_nofpu         Use code optimized for i386 processors with fixed point arithmetic (experimental)
  --with-cpu=i486         Use code optimized for i486 processors (only usable alone!)
  --with-cpu=i586         Use code optimized for i586 processors
  --with-cpu=i586_dither  Use code optimized for i586 processors with dithering (noise shaping), adds 256K to binary size
  --with-cpu=3dnow        Use code optimized for 3DNow processors
  --with-cpu=3dnowext     Use code optimized for 3DNowExt processors (K6-3+, Athlon)
  --with-cpu=3dnowext_alone     Really only 3DNowExt decoder, without 3DNow fallback for flexible rate
  --with-cpu=mmx          Use code optimized for MMX processors
  --with-cpu=mmx_alone          Really only MMX decoder, without i586 fallback for flexible rate
  --with-cpu=sse          Use code optimized for SSE processors
  --with-cpu=sse_alone          Really only SSE decoder, without i586 fallback for flexible rate
  --with-cpu=x86          Pack all x86 opts into one binary (excluding i486, including dither)
  --with-cpu=x86-64       Use code optimized for x86-64 processors (AMD64 and Intel64, including dithered generic)
  --with-cpu=altivec      Use code optimized for Altivec processors (PowerPC G4 and G5)
  --with-cpu=ppc_nofpu    Use code optimized for PowerPC processors with fixed point arithmetic (experimental)
  --with-cpu=neon         Use code optimized for ARM NEON SIMD engine (Cortex-A series)
  --with-cpu=arm_nofpu    Use code optimized for ARM processors with fixed point arithmetic (experimental)
])


# opt-in for IEEE754 floating point action
# We want to make sure not to apply hacks relying on IEEE754 storage format on platforms where we did not make sure that this actually is valid.
# It _should_ be fine for about all computers still out there, but we play safe.
ieee=no
case $host in
  i?86-*|x86_64-*|powerpc*|sparc*)
    ieee=yes
  ;;
esac
if test "x$ieee" = xyes; then
  echo "We assume IEEE754 floating point format."
  AC_DEFINE(IEEE_FLOAT,   1, [ Define to indicate that float storage follows IEEE754. ])
fi


case $host in
  arm*-*-linux*)
    # check that... perhaps we are better off on arm with kernel math emulation
    cpu_type="arm_nofpu"
  ;;
  i386-*-linux*|i386-*-kfreebsd*-gnu)
    cpu_type="i386_fpu"
  ;;
  i486-*-linux*|i486-*-kfreebsd*-gnu)
    cpu_type="i486"
  ;;
  i586-*-linux*|i586-*-kfreebsd*-gnu)
    cpu_type="x86"
  ;;
  i686-*-linux*|i686-*-kfreebsd*-gnu)
    cpu_type="x86"
  ;;
  x86_64-*-linux*|x86_64-*-kfreebsd*-gnu)
    cpu_type="x86-64_all"
  ;;
  *-*-linux*|*-*-kfreebsd*-gnu)
    cpu_type="generic_fpu"
  ;;
  i386-apple-darwin10*)
    AC_MSG_CHECKING([if CPU type supports x86-64])
    case `sysctl -n hw.optional.x86_64` in
      1)
        AC_MSG_RESULT([yes])
        cpu_type="x86-64_all"
      ;;
      *)
        AC_MSG_RESULT([no])
        cpu_type="x86"
      ;;
    esac
  ;;
  i386-apple-darwin*)
    cpu_type="x86"
  ;;
  x86_64-apple-darwin*)
    cpu_type="x86-64_all"
  ;;
  *-apple-darwin*)
    AC_MSG_CHECKING([if CPU type supports AltiVec])
    case `machine` in
      ppc7400 | ppc7450 | ppc970)
        AC_MSG_RESULT([yes])
        cpu_type="altivec"
      ;;
      *)
        AC_MSG_RESULT([no])
        cpu_type="generic_fpu"
      ;;
    esac
  ;;
  i?86-*-dragonfly* | i?86-*-freebsd* | i?86-*-midnightbsd* | i?86-*-mirbsd* | i?86-*-netbsd* | i?86-*-openbsd*)
    cpu_type="x86"
  ;;
  x86_64-*-dragonfly* | x86_64-*-freebsd* | x86_64-*-midnightbsd* | x86_64-*-mirbsd* | x86_64-*-netbsd* | x86_64-*-openbsd*)
    cpu_type="x86-64_all"
  ;;
  *-*-dragonfly* | *-*-freebsd* | *-*-midnightbsd* | *-*-mirbsd* | *-*-netbsd* | *-*-openbsd*)
    cpu_type="generic_fpu"
  ;;
  i386-*-solaris*)
    cpu_type=x86
  ;;
  x86_64-*-solaris*)
    cpu_type=x86-64_all
  ;;
  *-*-solaris*)
    cpu_type="generic_fpu"
  ;;
  # os2-emx = OS/2 with some Unix fun; so p.ex. buffer works.
  # Till we sorted out the assembler troubles, generic CPU is default.
  i386-pc-os2-emx)
    cpu_type=generic_fpu
  ;;
  x86_64-pc-os2-emx)
    # We are optimistic hat the future knows OS/2 on x86-64;-)
    cpu_type=generic_fpu
  ;;
  *-pc-os2-emx)
    cpu_type="generic_fpu"
  ;;
  *-dec-osf*)
    cpu_type="generic_fpu"
  ;;
  x86_64-pc-cygwin*)
    cpu_type="x86-64_all"
  ;;
  i686-pc-cygwin*)
    cpu_type="x86"
  ;;
  i586-pc-cygwin*)
    cpu_type="x86"
  ;;
  i486-pc-cygwin*)
    cpu_type="i486"
  ;;
  i386-pc-cygwin*)
    cpu_type="i386"
  ;;
  *-pc-cygwin*)
    cpu_type="generic_fpu"
  ;;
  i@<:@3-7@:>@86-*-mingw32*)
    LIBS="$LIBS"
    buffer=disabled
    cpu_type="x86"
  ;;
  x86_64-*-mingw32*)
    LIBS="$LIBS"
    buffer=disabled
    cpu_type="x86-64"
  ;;
	i386-*)
		AC_MSG_WARN([Unknown host operating system])
		cpu_type="i386"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i486-*)
		AC_MSG_WARN([Unknown host operating system])
		cpu_type="i486"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i586-*)
		AC_MSG_WARN([Unknown host operating system])
		cpu_type="x86"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i686-*)
		AC_MSG_WARN([Unknown host operating system])
		cpu_type="x86"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	x86_64-*)
		AC_MSG_WARN([Unknown host operating system])
		cpu_type="x86-64_all"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
  *)
  	AC_MSG_WARN([Unknown host operating system])
    cpu_type="generic_fpu"
    buffer=disabled
    ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
  ;;
esac

AC_ARG_ENABLE(buffer,
  [  --enable-buffer=[yes/no] disable audio buffer code (default uses system whitelist... proper checks later) ],
  [
    if test "x$enableval" = xyes
    then
      echo "Note: Enabling buffer per request... perhaps it will not build anyway."
      buffer="enabled"
    else
      echo "Note: Disabling buffer per request."
      buffer="disabled"
    fi
  ]
)

dnl Did user choose other CPU type ?
if test "x$with_cpu" != "x"; then
	cpu_type=$with_cpu
fi

if test "x$cpu_type" = "xgeneric_nofpu"; then
  real=disabled
  int32=disabled
fi

if test "x$cpu_type" = "xi386_nofpu"; then
  real=disabled
  int32=disabled
fi

if test "x$cpu_type" = "xppc_nofpu"; then
  real=disabled
  int32=disabled
fi

if test "x$cpu_type" = "xarm_nofpu"; then
  real=disabled
  int32=disabled
fi

if test "x$int16" = "xdisabled"; then
  AC_DEFINE(NO_16BIT, 1, [ Define to disable 16 bit integer output. ])
else
  DECODER_OBJ="$DECODER_OBJ synth.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ synth.lo"
fi

# 8bit works only through 16bit
if test "x$int16" = "xdisabled"; then
  int8=disabled
fi
if test "x$int8" = "xdisabled"; then
  AC_DEFINE(NO_8BIT, 1, [ Define to disable 8 bit integer output. ])
else
  DECODER_OBJ="$DECODER_OBJ synth_8bit.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ synth_8bit.lo"
fi

if test "x$int32" = "xdisabled"; then
  AC_DEFINE(NO_32BIT, 1, [ Define to disable 32 bit and 24 bit integer output. ])
else
  s_fpu="$s_fpu synth_s32"
fi

if test "x$real" = "xdisabled"; then
  AC_DEFINE(NO_REAL, 1, [ Define to disable real output. ])
else
  s_fpu="$s_fpu synth_real"
fi



layer1=enabled
AC_ARG_ENABLE(layer1,
              [  --disable-layer1=[no/yes] no layer I decoding ],
              [
                if test "x$enableval" = xno; then
                  layer1="disabled"
                fi
              ], [])

if test "x$layer1" = "xdisabled"; then
  AC_DEFINE(NO_LAYER1, 1, [ Define to disable layer I. ])
else
  # layer1 needs code in layer2
  DECODER_OBJ="$DECODER_OBJ layer1.\$(OBJEXT) layer2.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ layer1.lo layer2.lo"
fi

layer2=enabled
AC_ARG_ENABLE(layer2,
              [  --disable-layer2=[no/yes] no layer II decoding ],
              [
                if test "x$enableval" = xno; then
                  layer2="disabled"
                fi
              ], [])

if test "x$layer2" = "xdisabled"; then
  AC_DEFINE(NO_LAYER2, 1, [ Define to disable layer II. ])
else
  # layer1 may have added the objects already
  if test "x$layer1" = "xdisabled"; then
    DECODER_OBJ="$DECODER_OBJ layer2.\$(OBJEXT)"
    DECODER_LOBJ="$DECODER_LOBJ layer2.lo"
  fi
fi

layer3=enabled
AC_ARG_ENABLE(layer3,
              [  --disable-layer3=[no/yes] no layer III decoding ],
              [
                if test "x$enableval" = xno; then
                  layer3="disabled"
                fi
              ], [])

if test "x$layer3" = "xdisabled"; then
  AC_DEFINE(NO_LAYER3, 1, [ Define to disable layer III. ])
else
  DECODER_OBJ="$DECODER_OBJ layer3.\$(OBJEXT)"
  DECODER_LOBJ="$DECODER_LOBJ layer3.lo"
fi

AC_ARG_WITH([audio], [
  --with-audio=<list of modules>  Select a list (or only one) of audio output modules (comma or space separated list).
])
AC_ARG_WITH([default-audio], [
  --with-default-audio=aix        Use AIX as default audio output sub-system
  --with-default-audio=alib       Use Alib as default audio output sub-system (for HPUX)
  --with-default-audio=alsa       Use ALSA as default audio output sub-system
  --with-default-audio=arts       Use aRts as default audio output sub-system (KDE sound server)
  --with-default-audio=dummy      Use dummy as default audio (when no sound card is available)
  --with-default-audio=esd        Use ESoundD as default audio output sub-system
  --with-default-audio=hp         Use HP as default audio output sub-system
  --with-default-audio=jack       Use JACK as default low-latency audio server
  --with-default-audio=coreaudio  Use Mac OS X as default audio output sub-system (CoreAudio)
  --with-default-audio=mint       Use MinT as default audio output sub-system (Atari)
  --with-default-audio=nas        Use NAS as default audio output (Network Audio System)
  --with-default-audio=os2        Use OS2 as default audio output sub-system
  --with-default-audio=oss        Use OSS as default audio output sub-system (/dev/dsp)
  --with-default-audio=portaudio  Use PortAudio as default audio output sub-system
  --with-default-audio=pulse      Use Pulse audio server as default audio output sub-system
  --with-default-audio=sdl        Use SDL as default audio output sub-system (Simple DirectMedia Layer)
  --with-default-audio=sgi        Use SGI as default audio output sub-system (IRIX)
  --with-default-audio=sndio      Use OpenBSD's sndio as default audio output sub-system
  --with-default-audio=sun        Use Sun as default audio output sub-system (/dev/audio)
  --with-default-audio=win32      Use Win32 audio as default audio output sub-system
])

AC_ARG_WITH([optimization], [
  --with-optimization=0   No Optimization
  --with-optimization=1   Limited Optimization (-O) (for gcc)
  --with-optimization=2   Default Optimization (-O2 ...) (for gcc)
  --with-optimization=3   More Optimize than default (-O3 ...) (for gcc)
  --with-optimization=4   Optimize yet more (-O4 ...) (for gcc)
])

AC_ARG_WITH([seektable], [
  --with-seektable=<size> choose size of seek index table (0 disables it), default 1000
])


dnl ############## Modules

# Dummy audio output module is always supported
output_modules="dummy"

# Decide on module file suffix. Setting it != .la allows removal of .la files.
AC_ARG_WITH([module-suffix], [
  --with-module-suffix=.la            (default) Use libtool's .la files for loading modules.
  --with-module-suffix=[.so|.dll|...] Specify the native module file suffix to bypass .la files.
])
if test x"$with_module_suffix" = x; then
  with_module_suffix=.la
fi
AC_DEFINE_UNQUOTED( MODULE_FILE_SUFFIX, "$with_module_suffix", [The suffix for module files.] )

dnl ############## Assembler, compiler properties

# based on posting from John Dalgliesh <johnd@defyne.org> on ffmpeg (LGPL) mailing list
# find if .align arg is power-of-two or not
asmalign_exp="unknown"
if test x"$asmalign_exp" = xunknown; then
	AC_MSG_CHECKING([if .align takes 2-exponent])
	asmalign_exp="no"
	echo '.align 3' > conftest.s
	if $CCAS -c -o conftest.o conftest.s 1>/dev/null 2>&1; then
		asmalign_exp="yes"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	rm -f conftest.o conftest.s
fi
if test x"$asmalign_exp" = xyes; then
	AC_DEFINE(ASMALIGN_EXP, 1, [ Define if .align takes 3 for alignment of 2^3=8 bytes instead of 8. ])
fi

ccalign="unknown"
if test x"$ccalign" = xunknown; then
	AC_MSG_CHECKING([__attribute__((aligned(16)))])
	ccalign="no"
	echo '__attribute__((aligned(16))) float var;' > conftest.c
	if $CC -c -o conftest.o conftest.c >/dev/null 2>&1; then
		ccalign="yes"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	rm -f conftest.o conftest.c
fi

dnl We apply alignment hints only to cpus that need it.
dnl See further below for the definition of CCALIGN

dnl ############## Really basic headers, needed for other checks.

AC_HEADER_STDC
dnl Is it too paranoid to specifically check for stdint.h and limits.h?
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h sched.h sys/ioctl.h sys/types.h stdint.h limits.h inttypes.h sys/time.h sys/wait.h sys/resource.h sys/signal.h signal.h])

dnl ############## Types

dnl Large file support stuff needs cleanup. There are superfluous variables.

dnl Detect large file support, enable switches if needed.
AC_SYS_LARGEFILE
dnl If we do have a switch for large files, rename off_t-aware API calls.
dnl Using the file_offset_bits variable here is fine for linux (possibly Solaris),
dnl Others... we'll have to see.
dnl Note: I started writing this with with multiline replacements.
dnl Does not work. Automake insists on putting these into Makefiles where they break things.
if test "x$ac_cv_sys_file_offset_bits" = x || echo "$ac_cv_sys_file_offset_bits" | $GREP  '@<:@^0-9@:>@' > /dev/null; then
	dnl if it has non-numeric chars or is empty... ignore...
	LFS_LOBJ=
else
	# Add dual-mode wrapper code.
	LFS_LOBJ=lfs_wrap.lo
fi

# Using the lower level macros instead of AC_TYPE_* for compatibility with not freshest autoconf.
AC_CHECK_TYPE(size_t, unsigned long)
AC_CHECK_TYPE(uintptr_t, unsigned long)
AC_CHECK_TYPE(ssize_t, long)
AC_CHECK_TYPE(off_t,  long int)
AC_CHECK_TYPE(int32_t, int)
AC_CHECK_TYPE(uint32_t, unsigned int)
AC_CHECK_TYPE(int16_t, short)
AC_CHECK_TYPE(uint16_t, unsigned short)
AC_CHECK_SIZEOF(size_t,4)
AC_CHECK_SIZEOF(ssize_t,4)
AC_CHECK_SIZEOF(off_t,4)
AC_CHECK_SIZEOF(int32_t)
AC_CHECK_SIZEOF(long,4)

# The alias functions want to know the native off_t bits.
# "Native" also means large file offsets, if enabled, it's what is native to the mpg123 library.
if test "x$ac_cv_sizeof_long" = "x"; then 
  AC_MSG_ERROR([Cannot determine sizeof(long)?])
else
  LFS_ALIAS_BITS=`expr "$ac_cv_sizeof_long" "*" "8"`
  AC_DEFINE_UNQUOTED([LFS_ALIAS_BITS], $LFS_ALIAS_BITS,
    [Define this to the size of long type in bits, used for LFS small/native alias functions.])
fi

lfs_alias=enabled
AC_ARG_ENABLE(lfs-alias,
              [  --disable-lfs-alias disable alias wrappers for largefile bitness (mpg123_seek_32 in addition to mpg123_seek, mpg123_seek_64 as alias on x86-64) ],
              [
                if test "x$enableval" = xno; then
                  lfs_alias="disabled"
                fi
              ],
              [
                case $host in
                *-cygwin*)
                  lfs_alias="disabled"
                  AC_MSG_NOTICE([lfs-alias disabled for Cygwin, use --enable-lfs-alias explicitly to enable, only if you know what you are doing])
                ;;
                esac

              ])

AC_MSG_CHECKING([if we want to enable alias wrappers for largefile])
if test "x$lfs_alias" = "xenabled"; then
  AC_MSG_RESULT([yes])
  LFS_LOBJ="$LFS_LOBJ lfs_alias.lo"
else
  AC_MSG_RESULT([no])
fi

AC_SUBST(LFS_LOBJ)


dnl ############## Function Checks

AC_FUNC_MMAP

# Check if system supports termios
AC_SYS_POSIX_TERMIOS
if test "x$ac_cv_sys_posix_termios" = "xyes"; then
  AC_DEFINE_UNQUOTED([HAVE_TERMIOS], 1, 
  [Define this if you have the POSIX termios library])
fi

AC_CHECK_FUNCS( random )

# Check for sched_setscheduler
AC_CHECK_FUNCS( sched_setscheduler setuid getuid)

# Check for setpriority
AC_CHECK_FUNCS( setpriority )

AC_CHECK_FUNCS( strerror )

AC_CHECK_FUNCS( setlocale nl_langinfo )

AC_CHECK_FUNCS( strdup )

AC_CHECK_FUNCS( atoll )

AC_CHECK_FUNCS( mkfifo, [ have_mkfifo=yes ], [ have_mkfifo=no ] )

if test x"$fifo" = xauto; then
	if test x"$have_mkfifo" = xyes; then
		fifo=enabled
	else
		fifo=disabled
	fi
fi
if test x"$fifo" = xenabled; then
	AC_DEFINE(FIFO, 1, [ Define if FIFO support is enabled. ])
	if test x"$have_mkfifo" = xno; then
		AC_MSG_WARN( [ You forced FIFO code while I think there is no mkfifo() available! ] )
	fi
fi

dnl ############## Header and Library Checks

# locale headers
AC_CHECK_HEADERS([locale.h langinfo.h])
# Headers for network (http) stuff
network_type=Unknown
AC_CHECK_HEADERS([netdb.h sys/param.h sys/socket.h netinet/in.h arpa/inet.h])
if test "x$ac_cv_header_netdb_h" = "xyes"      &&
   test "x$ac_cv_header_sys_param_h" = "xyes"  &&
   test "x$ac_cv_header_sys_socket_h" = "xyes" &&
   test "x$ac_cv_header_netinet_in_h" = "xyes" &&
   test "x$ac_cv_header_arpa_inet_h"  = "xyes"; then
  have_network=yes
  network_type=Posix
else
  have_network=no
fi

dnl trying to get that socket lib settled in one line
AC_SEARCH_LIBS(gethostbyname, nsl socket) 
dnl OK, two lines... Solaris needs -lnsl -lsocket
AC_SEARCH_LIBS(socket, socket) 

AC_CHECK_FUNCS( getaddrinfo, [ have_ipv6=yes ], [ have_ipv6=no ] )

APR_CHECK_GETADDRINFO_ADDRCONFIG()

# Substitutions for the installable mpg123.h header
if test "x$ac_cv_header_stdio_h" = "xyes"; then
	INCLUDE_STDIO_H="#include <stdio.h>"
else
	INCLUDE_STDIO_H="/* #include <stdio.h> is not available on this system */"
fi
AC_SUBST(INCLUDE_STDIO_H)

if test "x$ac_cv_header_stdlib_h" = "xyes"; then
	INCLUDE_STDLIB_H="#include <stdlib.h>"
else
	INCLUDE_STDLIB_H="/* #include <stdlib.h> is not available on this system */"
fi
AC_SUBST(INCLUDE_STDLIB_H)

if test "x$ac_cv_header_sys_types_h" = "xyes"; then
	INCLUDE_SYS_TYPE_H="#include <sys/types.h>"
else
	INCLUDE_SYS_TYPE_H="/* #include <sys/types.h> is not available on this system */"
fi
AC_SUBST(INCLUDE_SYS_TYPE_H)


# Checks for maths libraries.
AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([mx], [powf])

# attempt to make the signal stuff work... also with GENERIC - later
#if test x"$ac_cv_header_sys_signal_h" = xyes; then
#	AC_CHECK_FUNCS( sigemptyset sigaddset sigprocmask sigaction )
#	if test x"$ac_cv_func_sigemptyset" = xyes &&
#	   test x"$ac_cv_func_sigaddset" = xyes &&
#	   test x"$ac_cv_func_sigprocmask" = xyes &&
#	   test x"$ac_cv_func_sigaction" = xyes; then
#	AC_DEFINE( 
#fi

dnl ############## Choose compiler flags and CPU

# do not assume gcc here, so no flags by default
ADD_CFLAGS=""
ADD_CPPFLAGS=""
ADD_LDFLAGS=""
LIBS="$LIBS"

AC_CHECK_HEADER([os2.h], [ADD_CPPFLAGS="$ADD_CPPFLAGS -DOS2"])

# If debugging is enabled then make warnings errors
if test x"$debugging" = xenabled; then
	ADD_CFLAGS="-g"
	# gcc specific...
	if test x"$GCC" = xyes; then
		ADD_CFLAGS="$ADD_CFLAGS -Wall -fno-strict-aliasing"
		if test x"$nagging" = xenabled; then
			ADD_CFLAGS="$ADD_CFLAGS -Werror -pedantic -DPLAIN_C89"
		fi
	fi
fi

dnl Only try the attribute_align_arg mumbo-jumbo on x86, x86-64 warns/errors out on that attribute.
dnl ...dunno even what about other architectures.
case $host in
  i?86-*)
    AC_DEFINE(ABI_ALIGN_FUN, 1, [ Define if your architecture wants/needs/can use attribute_align_arg and alignment checks. It's for 32bit x86... ])
 ;;
esac

s_altivec="synth_altivec dct64_altivec"
s_i386="dct64_i386"
s_i486="$s_i386 synth_i486 dct64_i486"
s_i586="$s_i386 synth_i586"
s_i586d="$s_i386 synth_i586_dither"
s_3dnow="$s_i386 synth_3dnow dct64_3dnow equalizer_3dnow"
s_3dnowext="$s_i386 dct64_3dnowext tabinit_mmx synth_3dnowext"
if test "x$layer3" = "xenabled"; then
  s_3dnow="$s_3dnow dct36_3dnow"
  s_3dnowext="$s_3dnowext dct36_3dnowext"
fi
s_mmx="$s_i386 dct64_mmx tabinit_mmx synth_mmx"
s_sse="$s_i386 tabinit_mmx dct64_sse dct64_sse_float synth_sse_float synth_stereo_sse_float synth_sse_s32 synth_stereo_sse_s32 "
s_x86_64="dct64_x86_64 dct64_x86_64_float synth_x86_64_float synth_x86_64_s32 synth_stereo_x86_64_float synth_stereo_x86_64_s32"
s_x86multi="getcpuflags"
s_dither="dither"
s_neon="dct64_neon dct64_neon_float synth_neon_float synth_neon_s32 synth_stereo_neon_float synth_stereo_neon_s32"

# choose optimized 16bit decoder for SSE, quality or fast
# note: supporting deactivation of output formats for these decoders would need more logic here
if test "x$integers" = "xquality"; then
  s_sse="$s_sse synth_sse_accurate synth_stereo_sse_accurate"
  s_x86_64="$s_x86_64 synth_x86_64_accurate synth_stereo_x86_64_accurate"
  s_arm="synth_arm_accurate"
  s_neon="$s_neon synth_neon_accurate synth_stereo_neon_accurate"
else
  s_sse="$s_sse synth_sse" # no stereo
  s_x86_64="$s_x86_64 synth_x86_64 synth_stereo_x86_64"
  s_arm="synth_arm"
  s_neon="$s_neon synth_neon synth_stereo_neon"
fi

dnl CPU specific compiler flags and sources
case $cpu_type in
  generic)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FLOAT"
    more_sources="$s_fpu"
    ccalign=no
  ;;
  generic_dither)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC_DITHER -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_dither"
    ccalign=no
  ;;
dnl Not disabling buffer for float mode... check that this is OK now!
  generic_fpu | generic_float)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FLOAT"
    more_sources="$s_fpu"
    ccalign=no
  ;;
  generic_nofpu)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FIXED"
    more_sources=
    ccalign=no
  ;;
  ppc_nofpu)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DOPT_PPC -DREAL_IS_FIXED"
    more_sources=
    ccalign=no
  ;;
  arm_nofpu)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_ARM -DREAL_IS_FIXED"
    more_sources="$s_arm"
    ccalign=no
  ;;
  altivec)
    ADD_CFLAGS="$ADD_CFLAGS -maltivec"
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DREAL_IS_FLOAT -DOPT_ALTIVEC"
    more_sources="$s_altivec $s_fpu"
    AC_MSG_CHECKING([if $CC accepts -faltivec])
    touch conftest.c
    if $CC -faltivec -c -o conftest.o conftest.c >/dev/null 2>&1; then
        ADD_CFLAGS="$ADD_CFLAGS -faltivec"
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
    fi
    rm -f conftest.o conftest.c
  ;;
  neon)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_NEON -DREAL_IS_FLOAT"
    more_sources="$s_neon $s_fpu"
  ;;
  i386) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i386"
    ccalign=no
  ;;
  i386_fpu) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i386"
    ccalign=no
  ;;
  i386_nofpu) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FIXED"
    more_sources="$s_i386"
    ccalign=no
  ;;
  i486) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I486 -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i486"
    ccalign=no
  ;;
  i586) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I586 -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i586"
  ;;
  i586_dither) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I586_DITHER -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i586d $s_dither"
  ;;
  3dnow)
    # legacy 3dnow had the 3dnow paired with i586...
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_I586 -DOPT_3DNOW -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i586 $s_3dnow $s_x86multi "
  ;;
  3dnow_alone)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_3DNOW -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_3dnow"
  ;;
  3dnowext_alone) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_3DNOWEXT -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_3dnowext"
  ;;
  3dnowext) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_3DNOW -DOPT_3DNOWEXT -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_3dnowext $s_3dnow $s_x86multi"
  ;;
  mmx_alone|mmx) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MMX -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_mmx"
  ;;
  sse_alone|sse) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_sse"
  ;;
  x86|x86_dither) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_GENERIC -DOPT_GENERIC_DITHER -DOPT_I386 -DOPT_I586 -DOPT_I586_DITHER -DOPT_MMX -DOPT_3DNOW -DOPT_3DNOWEXT -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_i386 $s_i586 $s_i586d $s_mmx $s_3dnow $s_3dnowext $s_sse $s_x86multi $s_dither"
  ;;
  x86-64) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_X86_64 -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_x86_64"
  ;;
  x86-64_all|x86-64_dither)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_X86_64 -DOPT_GENERIC -DOPT_GENERIC_DITHER -DREAL_IS_FLOAT"
    more_sources="$s_fpu $s_x86_64 $s_dither"
  ;;
  *)
  	AC_MSG_ERROR([Unknown CPU type '$cpu_type'])
  ;;
esac

# Mac OS X specific linker flags
case $cpu_type in
  3dnow|3dnow_alone|3dnowext|3dnowext_alone|mmx|mmx_alone|sse|sse_alone|x86|x86_dither)
    case $host in
      i386-apple-darwin8*|*-apple-darwin10*)
        ADD_LDFLAGS="$ADD_LDFLAGS -Wl,-read_only_relocs,suppress"
      ;;
      i386-apple-darwin9*)
        ADD_CFLAGS="$ADD_CFLAGS -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
        ADD_LDFLAGS="$ADD_LDFLAGS -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -Wl,-classic_linker -Wl,-read_only_relocs,suppress"
      ;;
    esac
  ;;
esac

# Check if we want feature report function.
AC_MSG_CHECKING([if we want feature report function])
feature_report=enabled
AC_ARG_ENABLE(feature_report,
	[  --disable-feature_report Disable feature report function ],
	[
		if test "x$enableval" = xno; then
		AC_MSG_RESULT([no])
		feature_report=disabled
		else
		AC_MSG_RESULT([yes])
		fi
	],
	[AC_MSG_RESULT([yes])])

if test "x$feature_report" = xenabled; then
	more_sources="$more_sources feature"
fi

for i in $more_sources
do
  # Echo make sure every file is only once in the list.
  echo "$DECODER_OBJ" | $GREP -v -w "$i\.\$(OBJEXT)" > /dev/null && DECODER_OBJ="$DECODER_OBJ $i.\$(OBJEXT)"
  echo "$DECODER_LOBJ" | $GREP -v -w "$i\.lo" > /dev/null && DECODER_LOBJ="$DECODER_LOBJ $i.lo"
done

AC_SUBST(DECODER_OBJ)
AC_SUBST(DECODER_LOBJ)

dnl Finally insert the code switch for alignment, above cpu selection having possibly changed the setting.
if test x"$ccalign" = xyes; then
	AC_DEFINE(CCALIGN, 1, [ Define if __attribute__((aligned(16))) shall be used ])
fi


dnl ############## Output module choice

# The full list of supported modules to check, first come, first serve.
check_modules="alsa oss coreaudio sndio sun win32 os2 esd jack portaudio pulse sdl nas arts openal dummy"
# The final list.
output_modules=

check_forced=no
check_failed=no
if test "x$with_audio" != "x"; then
	check_modules="`echo $with_audio|tr , ' '` dummy"
	echo "Limiting outputs to build according to your preference: $check_modules"
	check_forced=yes
fi

PKG_PROG_PKG_CONFIG([])

# Now go through the modules to check and do the chores.
for m in $check_modules
do
	case "$m" in
		dummy)
			AC_MSG_CHECKING([if you are too dumbing dumb for the dummy])
			AC_MSG_RESULT([no])
			output_modules="$output_modules dummy"
			HAVE_DUMMY=yes
		;;
		oss)
			AC_CHECK_HEADERS([sys/soundcard.h linux/soundcard.h machine/soundcard.h])
			if test "x${ac_cv_header_sys_soundcard_h}" = "xyes" \
				 -o "x${ac_cv_header_linux_soundcard_h}" = "xyes" \
				 -o "x${ac_cv_header_machine_soundcard_h}" = "xyes";
			then
				output_modules="$output_modules oss"
				HAVE_OSS="yes"
			else
				check_failed=yes
			fi
		;;
		alsa)
			ALSA_LIBS="-lasound"
			# Check for ALSA
			AC_CHECK_LIB( [asound], [snd_pcm_open],
				[ AC_CHECK_HEADER( [alsa/asoundlib.h],
					[ output_modules="$output_modules alsa" HAVE_ALSA="yes"],
					[ AC_MSG_WARN([Found ALSA library but NOT header files on your system]) ] )
				]
			)
			if test "x$HAVE_ALSA" != xyes; then
				check_failed=yes
			fi
		;;
		jack)
			PKG_CHECK_MODULES(JACK, jack, output_modules="$output_modules jack" HAVE_JACK="yes", HAVE_JACK="no" check_failed=yes)
		;;
		pulse)
			PKG_CHECK_MODULES(PULSE, libpulse-simple, output_modules="$output_modules pulse" HAVE_PULSE="yes", HAVE_PULSE="no" check_failed=yes)
		;;
		esd)
			PKG_CHECK_MODULES(ESD, esound, output_modules="$output_modules esd" HAVE_ESD="yes", HAVE_ESD="no" check_failed=yes)
		;;
		portaudio)
			# Remember: This looks only insane because you chose an insane tab width!
			PORTAUDIO_LIBS="-lportaudio"
			PORTAUDIO_CFLAGS=
			case $host in
				*-*-mingw32)
					# We tested portaudio with MME
					if test "x$HAVE_PORTAUDIO" != "xyes"; then
						AC_CHECK_LIB( [portaudio], [Pa_Initialize],
							[ AC_CHECK_HEADER( [portaudio.h],
								[ output_modules="$output_modules portaudio" HAVE_PORTAUDIO_WINMM="yes" HAVE_PORTAUDIO="yes" ],
								[ AC_MSG_WARN([Found PortAudio library but NOT header files on your system]) ] )
								PORTAUDIO_LIBS="$PORTAUDIO_LIBS -lwinmm"
							],
							[ HAVE_PORTAUDIO="no"],
							[ -lwinmm ]
						)
					fi
				;;
				*)
					AC_CHECK_LIB( [portaudio], [Pa_Initialize],
						[ AC_CHECK_HEADER( [portaudio.h],
							[ output_modules="$output_modules portaudio" HAVE_PORTAUDIO="yes" ],
							[ AC_MSG_WARN([Found PortAudio library but NOT header files on your system]) ] )
						]
					)
				;;
			esac
			if test "x$HAVE_PORTAUDIO" != xyes; then
				check_failed=yes
			else
				# See if we have v19 or v18
				AC_CHECK_LIB( [portaudio], [Pa_GetVersion], [:], [AC_DEFINE( [PORTAUDIO18], 1, [Define if portaudio v18 API is wanted.]) ], [$PORTAUDIO_LIBS] )
			fi
		;;
		sdl)
			PKG_CHECK_MODULES(SDL, sdl, output_modules="$output_modules sdl" HAVE_SDL="yes", HAVE_SDL="no" check_failed=yes)
		;;
		nas)
			NAS_LIBS=-laudio
			AC_CHECK_LIB( [audio], [AuOpenServer],
				[ AC_CHECK_HEADER( [audio/audiolib.h],
					[ output_modules="$output_modules nas" HAVE_NAS="yes"],
					[ AC_MSG_WARN([Found NAS library but NOT header files on your system]) ] )
				]
			)
			if test "x$HAVE_NAS" != xyes; then
				check_failed=yes
			fi
		;;
		win32)
			# Check for windows ... and win32 audio
			# Does not work... instead just check for header
			# AC_CHECK_LIB( [winmm], [waveOutOpen] )
			WIN32_LIBS=-lwinmm
			AC_CHECK_HEADERS([windows.h], output_modules="$output_modules win32" HAVE_WIN32=yes, HAVE_WIN32=no check_failed=yes)
		;;
		sndio)
			SNDIO_LIBS=-lsndio
			AC_CHECK_LIB([sndio], [sio_open],
				[AC_CHECK_HEADERS([sndio.h],
					[output_modules="$output_modules sndio" HAVE_SNDIO="yes"])
				]
			)
			if test "x$HAVE_SNDIO" != xyes; then
				check_failed=yes
			fi
		;;
		sun)
			AC_CHECK_HEADERS([sun/audioio.h sys/audioio.h asm/audioio.h sys/audio.h])
			if test "x${ac_cv_header_sun_audioio_h}" = "xyes" \
				 -o "x${ac_cv_header_sys_audioio_h}" = "xyes" \
				 -o "x${ac_cv_header_asm_audioio_h}" = "xyes";
			then
				output_modules="$output_modules sun"
				HAVE_SUN="yes"
			else
				check_failed=yes
			fi
		;;
		coreaudio)
			COREAUDIO_LIBS="-framework AudioToolbox -framework AudioUnit -framework CoreServices"
			AC_CHECK_HEADERS([AudioUnit/AudioUnit.h CoreServices/CoreServices.h AudioToolbox/AudioToolbox.h])
			if test "x${ac_cv_header_AudioUnit_AudioUnit_h}" = "xyes" \
				 -a "x${ac_cv_header_CoreServices_CoreServices_h}" = "xyes" \
				 -a "x${ac_cv_header_AudioToolbox_AudioToolbox_h}" = "xyes";
			then
				if test x"$modules" = xdisabled; then
					AC_MSG_WARN([Disabling buffer because of directly linked CoreAudio! Use the module if you need the buffer.])
					buffer=disabled
				fi
				output_modules="$output_modules coreaudio"
				HAVE_COREAUDIO="yes"
			else
				check_failed=yes
			fi
		;;
		arts)
			AC_MSG_CHECKING([for artsc])
			if artsc-config; then
				AC_MSG_RESULT([yes])
				output_modules="$output_modules arts"
				HAVE_ARTS=yes
				ARTS_LIBS=`artsc-config --libs`
				ARTS_CFLAGS=`artsc-config --cflags`
			else
				AC_MSG_RESULT([no])
				check_failed=yes
			fi
		;;
		openal)
			AC_CHECK_HEADERS([OpenAL/al.h OpenAL/alc.h AL/al.h AL/alc.h al.h alc.h])
			if test "x${ac_cv_header_OpenAL_al_h}" = "xyes" \
				 -a "x${ac_cv_header_OpenAL_alc_h}" = "xyes";
			then #Mac OS X
				output_modules="$output_modules openal"
				OPENAL_LIBS="-framework OpenAL"
				OPENAL_CFLAGS="-DOPENAL_SUBDIR_OPENAL"
				HAVE_OPENAL="yes"
			elif test "x${ac_cv_header_AL_al_h}" = "xyes" \
				 -a "x${ac_cv_header_AL_alc_h}" = "xyes";
			then #Linux
				output_modules="$output_modules openal"
				OPENAL_LIBS="-lopenal"
				OPENAL_CFLAGS="-DOPENAL_SUBDIR_AL"
				HAVE_OPENAL="yes"
			elif test "x${ac_cv_header_al_h}" = "xyes" \
				 -a "x${ac_cv_header_alc_h}" = "xyes";
			then #Windows?
				output_modules="$output_modules openal"
				OPENAL_LIBS="-lopenal"
				OPENAL_CFLAGS=""
				HAVE_OPENAL="yes"
			else
				check_failed=yes
			fi
		;;
		os2)
			OS2_LIBS="-los2me -lmmpm2 -lsocket"
			AC_CHECK_HEADERS([os2.h])
			# os2me.h depends on os2.h
			# Yes, that way of coding it is ugly.
			if test "x${ac_cv_header_os2_h}" = xyes
			then
				# We mimick exactly the way how the header will be used.
				# It seems to be picky...
				AC_CHECK_HEADERS([os2me.h], [], [], [#define INCL_OS2MM
#define INCL_DOS
#define INCL_VIO
#define INCL_KBD
#include <os2.h>
#])
			fi
			if test "x${ac_cv_header_os2_h}" = xyes \
			   -a   "x${ac_cv_header_os2me_h}" = xyes
			then
				output_modules="$output_modules os2"
				HAVE_OS2=yes
			else
				check_failed=yes
			fi
		;;
# from here on only forced tests, untested code
		hp)
			# What's the deal with that and alib?
			UNSUPPORTED_AUDIO=yes
			AC_CHECK_HEADER([sys/audio.h], [output_modules="$output_modules hp" HAVE_HP=yes], [check_failed=yes])
		;;
		alib)
			UNSUPPORTED_AUDIO=yes
			# ALIB_CFLAGS="-I/opt/audio/include"
			ALIB_LIBS=-lAlib
			# These headers may not be all about audio but they are used.
			AC_CHECK_HEADERS([ Alib.h CUlib.h netdb.h netinet/in.h netinet/tcp.h])
			if test "x${ac_cv_header_Alib_h}" = xyes \
			   -a   "x${ac_cv_header_CUlib_h}" = xyes \
			   -a   "x${ac_cv_header_netdb_h}" = xyes \
			   -a   "x${ac_cv_header_netinet_in_h}" = xyes \
			   -a   "x${ac_cv_header_netinet_tcp_h}" = xyes
			then
				output_modules="$output_modules alib"
				HAVE_ALIB=yes
			else
				check_failed=yes
			fi
		;;
		mint)
			UNSUPPORTED_AUDIO=yes
			AC_CHECK_HEADERS([audios.h], [output_modules="$output_modules mint" HAVE_MINT=yes], [check_failes=yes])
		;;
		aix)
			UNSUPPORTED_AUDIO=yes
			AC_CHECK_HEADERS([sys/audio.h], [output_modules="$output_modules aix" HAVE_AIX=yes], [check_failed=yes])
		;;
		sgi)
			UNSUPPORTED_AUDIO=yes
			SGI_LIBS=-laudio
			AC_CHECK_HEADER([dmedia/audio.h], [output_modules="$output_modules sgi" HAVE_SGI=yes], [check_failed=yes])
		;;
		*)
			AC_MSG_ERROR([Unsupported/-known output '$m' demanded!])
		;;
	esac
done

if test "x$check_forced" = xyes -a "x$UNSUPPORTED_AUDIO" = xyes; then
	AC_MSG_WARN([You requested bulding of an unsupported audio module. Be prepared for happy hacking and please tell us about your experience!])
fi

if test "x$check_forced" = xyes -a "x$check_failed" = "xyes"; then
	AC_MSG_ERROR([One/some of your requested audio modules failed the test!])
fi

# When you extend check_modules, you should extend this:
#for i in alsa oss coreaudio sndio sun win32 esd jack portaudio pulse sdl nas aix alib arts hp os2 sgi mint openal dummy; do echo $i; done |
#perl -ne 'chomp; $big = uc($_); print <<EOT;
#AC_SUBST(${big}_LIBS)
#AC_SUBST(${big}_LDFLAGS)
#AC_SUBST(${big}_CFLAGS)
#AM_CONDITIONAL( [HAVE_$big], [test "x\$HAVE_$big" = xyes] )
#EOT
#'
AC_SUBST(ALSA_LIBS)
AC_SUBST(ALSA_LDFLAGS)
AC_SUBST(ALSA_CFLAGS)
AM_CONDITIONAL( [HAVE_ALSA], [test "x$HAVE_ALSA" = xyes] )
AC_SUBST(OSS_LIBS)
AC_SUBST(OSS_LDFLAGS)
AC_SUBST(OSS_CFLAGS)
AM_CONDITIONAL( [HAVE_OSS], [test "x$HAVE_OSS" = xyes] )
AC_SUBST(COREAUDIO_LIBS)
AC_SUBST(COREAUDIO_LDFLAGS)
AC_SUBST(COREAUDIO_CFLAGS)
AM_CONDITIONAL( [HAVE_COREAUDIO], [test "x$HAVE_COREAUDIO" = xyes] )
AC_SUBST(SNDIO_LIBS)
AC_SUBST(SNDIO_LDFLAGS)
AC_SUBST(SNDIO_CFLAGS)
AM_CONDITIONAL( [HAVE_SNDIO], [test "x$HAVE_SNDIO" = xyes] )
AC_SUBST(SUN_LIBS)
AC_SUBST(SUN_LDFLAGS)
AC_SUBST(SUN_CFLAGS)
AM_CONDITIONAL( [HAVE_SUN], [test "x$HAVE_SUN" = xyes] )
AC_SUBST(WIN32_LIBS)
AC_SUBST(WIN32_LDFLAGS)
AC_SUBST(WIN32_CFLAGS)
AM_CONDITIONAL( [HAVE_WIN32], [test "x$HAVE_WIN32" = xyes] )
AC_SUBST(ESD_LIBS)
AC_SUBST(ESD_LDFLAGS)
AC_SUBST(ESD_CFLAGS)
AM_CONDITIONAL( [HAVE_ESD], [test "x$HAVE_ESD" = xyes] )
AC_SUBST(JACK_LIBS)
AC_SUBST(JACK_LDFLAGS)
AC_SUBST(JACK_CFLAGS)
AM_CONDITIONAL( [HAVE_JACK], [test "x$HAVE_JACK" = xyes] )
AC_SUBST(PORTAUDIO_LIBS)
AC_SUBST(PORTAUDIO_LDFLAGS)
AC_SUBST(PORTAUDIO_CFLAGS)
AM_CONDITIONAL( [HAVE_PORTAUDIO], [test "x$HAVE_PORTAUDIO" = xyes] )
AC_SUBST(PULSE_LIBS)
AC_SUBST(PULSE_LDFLAGS)
AC_SUBST(PULSE_CFLAGS)
AM_CONDITIONAL( [HAVE_PULSE], [test "x$HAVE_PULSE" = xyes] )
AC_SUBST(SDL_LIBS)
AC_SUBST(SDL_LDFLAGS)
AC_SUBST(SDL_CFLAGS)
AM_CONDITIONAL( [HAVE_SDL], [test "x$HAVE_SDL" = xyes] )
AC_SUBST(NAS_LIBS)
AC_SUBST(NAS_LDFLAGS)
AC_SUBST(NAS_CFLAGS)
AM_CONDITIONAL( [HAVE_NAS], [test "x$HAVE_NAS" = xyes] )
AC_SUBST(AIX_LIBS)
AC_SUBST(AIX_LDFLAGS)
AC_SUBST(AIX_CFLAGS)
AM_CONDITIONAL( [HAVE_AIX], [test "x$HAVE_AIX" = xyes] )
AC_SUBST(ALIB_LIBS)
AC_SUBST(ALIB_LDFLAGS)
AC_SUBST(ALIB_CFLAGS)
AM_CONDITIONAL( [HAVE_ALIB], [test "x$HAVE_ALIB" = xyes] )
AC_SUBST(ARTS_LIBS)
AC_SUBST(ARTS_LDFLAGS)
AC_SUBST(ARTS_CFLAGS)
AM_CONDITIONAL( [HAVE_ARTS], [test "x$HAVE_ARTS" = xyes] )
AC_SUBST(HP_LIBS)
AC_SUBST(HP_LDFLAGS)
AC_SUBST(HP_CFLAGS)
AM_CONDITIONAL( [HAVE_HP], [test "x$HAVE_HP" = xyes] )
AC_SUBST(OS2_LIBS)
AC_SUBST(OS2_LDFLAGS)
AC_SUBST(OS2_CFLAGS)
AM_CONDITIONAL( [HAVE_OS2], [test "x$HAVE_OS2" = xyes] )
AC_SUBST(SGI_LIBS)
AC_SUBST(SGI_LDFLAGS)
AC_SUBST(SGI_CFLAGS)
AM_CONDITIONAL( [HAVE_SGI], [test "x$HAVE_SGI" = xyes] )
AC_SUBST(MINT_LIBS)
AC_SUBST(MINT_LDFLAGS)
AC_SUBST(MINT_CFLAGS)
AM_CONDITIONAL( [HAVE_MINT], [test "x$HAVE_MINT" = xyes] )
AC_SUBST(OPENAL_LIBS)
AC_SUBST(OPENAL_LDFLAGS)
AC_SUBST(OPENAL_CFLAGS)
AM_CONDITIONAL( [HAVE_OPENAL], [test "x$HAVE_OPENAL" = xyes] )
AC_SUBST(DUMMY_LIBS)
AC_SUBST(DUMMY_LDFLAGS)
AC_SUBST(DUMMY_CFLAGS)
AM_CONDITIONAL( [HAVE_DUMMY], [test "x$HAVE_DUMMY" = xyes] )

# Did user choose default audio subsystem ?
if test "x$with_default_audio" != "x"; then
	default_output_module=$with_default_audio
else 
	default_output_module=`echo "$output_modules" | $AWK '{ print $1 }'`
fi

# That's (beginning of) the list for mpg123's internal default.
default_output_modules=$default_output_module

OUTPUT_OBJ=
OUTPUT_MOD=
OUTPUT_CFLAGS=
OUTPUT_LIBS=
OUTPUT_LDFLAGS=

# Setup the static build.
if test "x$modules" = xdisabled
then
	echo "Preparing static output $default_output_module"
	OUTPUT_MOD="$default_output_module"
	OUTPUT_OBJ="output/$default_output_module.\$(OBJEXT)"
	OUTPUT_CFLAGS=
	OUTPUT_LIBS=
	# That feels stupid... what about hashed arrays?
	case $OUTPUT_MOD in
	# Here's a script for that tedious list, perhaps to be outsourced together with the one in src/output/Makefile.am
#for i in alsa coreaudio esd jack nas oss portaudio pulse sdl sndio sun win32 aix alib arts hp os2 sgi mint; do echo $i; done |
#perl -ne 'chomp; $big = uc($_); print <<EOT;
#		$_)
#			OUTPUT_LIBS="\$${big}_LIBS"
#			OUTPUT_LDFLAGS="\$${big}_LDFLAGS"
#			OUTPUT_CFLAGS="\$${big}_CFLAGS"
#		;;
#EOT
#'
		alsa)
			OUTPUT_LIBS="$ALSA_LIBS"
			OUTPUT_LDFLAGS="$ALSA_LDFLAGS"
			OUTPUT_CFLAGS="$ALSA_CFLAGS"
		;;
		coreaudio)
			OUTPUT_LIBS="$COREAUDIO_LIBS"
			OUTPUT_LDFLAGS="$COREAUDIO_LDFLAGS"
			OUTPUT_CFLAGS="$COREAUDIO_CFLAGS"
		;;
		esd)
			OUTPUT_LIBS="$ESD_LIBS"
			OUTPUT_LDFLAGS="$ESD_LDFLAGS"
			OUTPUT_CFLAGS="$ESD_CFLAGS"
		;;
		jack)
			OUTPUT_LIBS="$JACK_LIBS"
			OUTPUT_LDFLAGS="$JACK_LDFLAGS"
			OUTPUT_CFLAGS="$JACK_CFLAGS"
		;;
		nas)
			OUTPUT_LIBS="$NAS_LIBS"
			OUTPUT_LDFLAGS="$NAS_LDFLAGS"
			OUTPUT_CFLAGS="$NAS_CFLAGS"
		;;
		oss)
			OUTPUT_LIBS="$OSS_LIBS"
			OUTPUT_LDFLAGS="$OSS_LDFLAGS"
			OUTPUT_CFLAGS="$OSS_CFLAGS"
		;;
		portaudio)
			OUTPUT_LIBS="$PORTAUDIO_LIBS"
			OUTPUT_LDFLAGS="$PORTAUDIO_LDFLAGS"
			OUTPUT_CFLAGS="$PORTAUDIO_CFLAGS"
		;;
		pulse)
			OUTPUT_LIBS="$PULSE_LIBS"
			OUTPUT_LDFLAGS="$PULSE_LDFLAGS"
			OUTPUT_CFLAGS="$PULSE_CFLAGS"
		;;
		sdl)
			OUTPUT_LIBS="$SDL_LIBS"
			OUTPUT_LDFLAGS="$SDL_LDFLAGS"
			OUTPUT_CFLAGS="$SDL_CFLAGS"
		;;
		sndio)
			OUTPUT_LIBS="$SNDIO_LIBS"
			OUTPUT_LDFLAGS="$SNDIO_LDFLAGS"
			OUTPUT_CFLAGS="$SNDIO_CFLAGS"
		;;
		sun)
			OUTPUT_LIBS="$SUN_LIBS"
			OUTPUT_LDFLAGS="$SUN_LDFLAGS"
			OUTPUT_CFLAGS="$SUN_CFLAGS"
		;;
		win32)
			OUTPUT_LIBS="$WIN32_LIBS"
			OUTPUT_LDFLAGS="$WIN32_LDFLAGS"
			OUTPUT_CFLAGS="$WIN32_CFLAGS"
		;;
		aix)
			OUTPUT_LIBS="$AIX_LIBS"
			OUTPUT_LDFLAGS="$AIX_LDFLAGS"
			OUTPUT_CFLAGS="$AIX_CFLAGS"
		;;
		alib)
			OUTPUT_LIBS="$ALIB_LIBS"
			OUTPUT_LDFLAGS="$ALIB_LDFLAGS"
			OUTPUT_CFLAGS="$ALIB_CFLAGS"
		;;
		arts)
			OUTPUT_LIBS="$ARTS_LIBS"
			OUTPUT_LDFLAGS="$ARTS_LDFLAGS"
			OUTPUT_CFLAGS="$ARTS_CFLAGS"
		;;
		hp)
			OUTPUT_LIBS="$HP_LIBS"
			OUTPUT_LDFLAGS="$HP_LDFLAGS"
			OUTPUT_CFLAGS="$HP_CFLAGS"
		;;
		os2)
			OUTPUT_LIBS="$OS2_LIBS"
			OUTPUT_LDFLAGS="$OS2_LDFLAGS"
			OUTPUT_CFLAGS="$OS2_CFLAGS"
		;;
		sgi)
			OUTPUT_LIBS="$SGI_LIBS"
			OUTPUT_LDFLAGS="$SGI_LDFLAGS"
			OUTPUT_CFLAGS="$SGI_CFLAGS"
		;;
		mint)
			OUTPUT_LIBS="$MINT_LIBS"
			OUTPUT_LDFLAGS="$MINT_LDFLAGS"
			OUTPUT_CFLAGS="$MINT_CFLAGS"
		;;
		openal)
			OUTPUT_LIBS="$OPENAL_LIBS"
			OUTPUT_LDFLAGS="$OPENAL_LDFLAGS"
			OUTPUT_CFLAGS="$OPENAL_CFLAGS"
		;;
	esac
else
	# Now make a comma-separated list again... eliminating the possible duplicate and dummy.
	for i in $output_modules
	do
		if test $i != $default_output_module && test $i != dummy; then
			default_output_modules=$default_output_modules,$i
		fi
	done
fi

AC_DEFINE_UNQUOTED( DEFAULT_OUTPUT_MODULE, "$default_output_modules", [The default audio output module(s) to use] )


AC_SUBST(OUTPUT_OBJ)
AC_SUBST(OUTPUT_MOD)
AC_SUBST(OUTPUT_CFLAGS)
AC_SUBST(OUTPUT_LIBS)
AC_SUBST(OUTPUT_LDFLAGS)

dnl ############## Compiler Optimizations

CFLAGS="$ADD_CFLAGS $CFLAGS"
if test x"$buffer" = xdisabled; then
	ADD_CPPFLAGS="$ADD_CPPFLAGS -DNOXFERMEM"
fi
CPPFLAGS="$ADD_CPPFLAGS $CPPFLAGS"
LDFLAGS="$ADD_LDFLAGS $LDFLAGS"

# None chosen?
if test "x$with_optimization" = "x"; then
	if test x"$debugging" = xenabled; then
		with_optimization="0"
	else
		# enable (gcc specific) default opts only with gcc
		if test "x$GCC" = xyes; then
			with_optimization="2"
		else
			with_optimization="0"
		fi
	fi
fi

case $with_optimization in
  0)
    # No Optimizations
    CFLAGS="$CFLAGS"
  ;;
  1)
    CFLAGS="-O $CFLAGS"
  ;;
  2)
    CFLAGS="-O2 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math $CFLAGS"
  ;;
  3)
    CFLAGS="-O3 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math $CFLAGS"
  ;;
  4)
    CFLAGS="-O4 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math $CFLAGS"
  ;;
  *)
  	AC_MSG_ERROR([Unknown optimizations level '$with_optimization'])
  ;;
esac

dnl If using gcc, prevent a bad alignment option from breaking things.
dnl Let's default to safe 16-byte alignment for any special x86 or altivec stuff.
if test "x$GCC" = xyes && echo "$CFLAGS" | $GREP 'mpreferred-stack-boundary=' > /dev/null; then
	case $cpu_type in
		x86 | i586* | mmx | sse | 3dnow* | altivec)
			AC_MSG_WARN( [ You are trying to mess with stack alignment. I know better. ] )
			CFLAGS="$CFLAGS -mpreferred-stack-boundary=4"
		;;
	esac
fi

dnl ############## Seektable size

if test "x$with_seektable" = "x"; then
	seektable=1000
else
	seektable=$with_seektable
fi

if test "$seektable" -gt 0; then
	AC_DEFINE(FRAME_INDEX, 1, [ Define if frame index should be used. ])
fi
# Define that always... also without frame index enabled.
AC_DEFINE_UNQUOTED(INDEX_SIZE, $seektable, [size of the frame index seek table])

dnl ############## Win32 function checks
# Check if we want Unicode for Win32. Cygwin does not need _wopen
win32_specific_codes=disabled
win32_unicode=unneeded
win32_sockets=disabled
win32_sockets_working=no
win32_wide_working=no
win32_winver_bump=no
AC_CHECK_HEADERS([windows.h], [win32_specific_codes=enabled])
if test "x$win32_specific_codes" = xenabled && test "x$host_os" != "xcygwin"; then

#### Check for Wide functions
  AC_CHECK_FUNC([_wopen], [win32_unicode=enabled],[win32_unicode=disabled])
  AC_MSG_CHECKING([if we want Unicode File Open for Win32])
  if test "x$win32_unicode" = xenabled; then
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([if Unicode functions working])
    AC_LINK_IFELSE([[
      #include <windows.h>
      #include <stdlib.h>
      int main()
      {
        MultiByteToWideChar (0, 0, NULL, 0, NULL, 0);
        WideCharToMultiByte (0, 0, NULL, 0, NULL, 0, NULL, NULL);
        return 0;
      }
    ]], [win32_wide_working=yes], [win32_winver_bump=yes])
    if test "x$win32_wide_working" = xno; then
      AC_LINK_IFELSE([[
        #define WINVER 0x501
        #define _WIN32_WINNT 0x501
        #include <windows.h>
        #include <stdlib.h>
        int main()
        {
          MultiByteToWideChar (0, 0, NULL, 0, NULL, 0);
          WideCharToMultiByte (0, 0, NULL, 0, NULL, 0, NULL, NULL);
          return 0;
        }
      ]], [win32_wide_working=yes], [AC_MSG_RESULT([no])])
    fi
    if test "x$win32_wide_working" = xyes; then
      AC_MSG_RESULT([yes])
      AC_DEFINE([WANT_WIN32_UNICODE], [1], [ Define to use Unicode for Windows ])
    fi
  else
    AC_MSG_WARN([Unicode File Open for Win32 disabled])
  fi

#### Check for Network functions
  AC_CHECK_HEADERS([ws2tcpip.h], [win32_sockets=enabled], [AC_MSG_WARN([Please update your headers to support winsock 2.2.])])
  AC_MSG_CHECKING([if we want Win32 sockets])
  if test "x$win32_sockets" = "xenabled" && test "x$network" != "xdisabled"; then
  AC_MSG_RESULT([yes])
  AC_MSG_CHECKING([if winsock2 API is available])
    wsoldlibs="$LIBS"
    LIBS="$LIBS -lws2_32"
    AC_LINK_IFELSE([[
      #include <winsock2.h>
      #include <ws2tcpip.h>
      #include <stdlib.h>
      int main()
      {
        getaddrinfo(NULL, NULL, NULL, NULL);
        freeaddrinfo(NULL);
        return 0;
      }
    ]], [win32_sockets_working=yes], [win32_winver_bump=yes])
    if test "x$win32_sockets_working" = "xno"; then
      AC_MSG_CHECKING([deeper if winsock2 API is available])
      AC_LINK_IFELSE([
        #define WINVER 0x501
        #define _WIN32_WINNT 0x501
        #include <winsock2.h>
        #include <ws2tcpip.h>
        #include <stdlib.h>
        int main()
        {
          getaddrinfo(NULL, NULL, NULL, NULL);
          freeaddrinfo(NULL);
          return 0;
        }
      ], [win32_sockets_working=yes], [AC_MSG_RESULT([no])])
    fi
    if test "x$win32_sockets_working" = "xyes"; then
      AC_MSG_RESULT([yes])
      AC_DEFINE([WANT_WIN32_SOCKETS], [1], [ Define to use Win32 sockets ])
      network_type=Winsock2
      have_network=yes
      have_ipv6=yes
    else
      LIBS="$wsoldlibs"
      AC_MSG_WARN([Please update your headers to support winsock 2.2.])
    fi
  else
    AC_MSG_RESULT([no])
  fi
  AC_MSG_CHECKING([if WINVER and _WIN32_WINNT needs version bumps])
  if test "x$win32_winver_bump" = "xyes"; then
    AC_MSG_RESULT([yes])
    CFLAGS="$CFLAGS -DWINVER=0x0501 -D_WIN32_WINNT=0x0501"
    # getaddrinfo and freeaddrinfo are available for Win2K and above
    # Bug: MinGW.org w32api ws2tcpip.h incorrectly wants (_WIN32_WINNT >= 0x0501) for getaddrinfo and freeaddrinfo
    # MultiByteToWideChar and WideCharToMultiByte are available for Win2K and above
  else
    AC_MSG_RESULT([no])
  fi
fi

#### Use Win32 support codes
AM_CONDITIONAL([WIN32_CODES], [ test "x$win32_specific_codes" = xenabled ])

if test x"$network" = xauto; then
	if test x"$have_network" = xyes; then
		network=enabled
	else
		network=disabled
	fi
fi
if test x"$ipv6" = xauto; then
	if test x"$have_ipv6" = xyes; then
		ipv6=enabled
	else
		ipv6=disabled
	fi
fi

dnl ############## Network enable
if test x"$network" = xenabled; then
	AC_DEFINE(NETWORK, 1, [ Define if network support is enabled. ])
	if test x"$have_network" = xno; then
		AC_MSG_WARN( [ You forced network code while I think there is support missing! ] )
	fi
fi

if test x"$ipv6" = xenabled; then
	AC_DEFINE(IPV6, 1, [ Define if IPV6 support is enabled. ])
	if test x"$have_ipv6" = xno; then
		AC_MSG_WARN( [ You forced IPv6 code while I think there is no getaddrinfo() available! ] )
	fi
fi

dnl ############## Final Output

# Do that here to prevent the above tests usign -lltdl...
if test "x$modules" = xenabled; then
  LIBS="-lltdl $LIBS"
fi

AC_CONFIG_FILES([
	Makefile
	libmpg123.pc
	mpg123.spec
	doc/Makefile
	src/Makefile
	src/output/Makefile
	src/libmpg123/Makefile
	src/libmpg123/mpg123.h
])

AC_OUTPUT



dnl ############## Display Message

echo "
  $PACKAGE_NAME $PACKAGE_VERSION

  Install path ............ $prefix
  CPU Optimization ........ $cpu_type
  Compiler Optimization ... $with_optimization
  Gapless Support ......... $gapless
  Debugging ............... $debugging
  Seek table size ......... $seektable
  FIFO support ............ $fifo
  Buffer .................. $buffer
  Network (http streams) .. $network
  Network Sockets ......... $network_type
  IPv6 (getaddrinfo) ...... $ipv6"
if test x"$LARGEFILE_BITS" = x; then
  echo "  File offsets ............ default"
else
  echo "  File offsets ............ $LARGEFILE_BITS"
  echo "    The lib will (try to) support default offset size, too."
fi
echo "  LFS alias symbols ....... $lfs_alias ($LFS_ALIAS_BITS)"

echo "  Alignment checks ........ $aligncheck"
if test x"$aligncheck" = xenabled; then
	if test x"$ccalign" != xyes; then
		echo "   (without effect since special alignment is not employed)"
	fi
	if test x"$GCC" = xyes; then
		echo "   (note that newer gcc 4.x versions do not use/need the align check)"
	fi
fi
echo -n "
  Core libmpg123 features:
  Integer conversion ...... $integers
  Layer I ................. $layer1
  Layer II ................ $layer2
  Layer III ............... $layer3
  NtoM resampling ......... $ntom
  downsampled decoding .... $downsample
  Feeder/buffered input ... $feeder
  ID3v2 parsing ........... $id3v2
  String API .............. $string
  ICY parsing/conversion .. $icy
  Error/warning messages .. $messages
  Win32 Unicode File Open.. $win32_unicode
  Feature Report Function.. $feature_report
  Output formats (nofpu will disable all but 16 or 8 bit!):
  8 bit integer ........... $int8
  16 bit integer .......... $int16
  32/24 bit integer ....... $int32
  real (32 bit float) ..... $real
"
# just an empty line
echo

echo "  Modules ................. $modules"
echo "  Module suffix ........... $with_module_suffix"
echo "  Checked audio modules ... $check_modules
  Detected audio support ..$output_modules
  Default output module ... $default_output_module
"
if test x"$modules" = xdisabled; then
echo "The _single_ active output module is being statically linked in.
"
fi
if test x"$with_optimization" = x0; then
	echo "No optimization flags chosen, make sure you have something basic in your CFLAGS at least...
"
fi
if test x"$debugging" = xenabled; then
	echo "CFLAGS='$CFLAGS'"
	echo "LIBS='$LIBS'"
else
    echo "Next type 'make' and then 'make install'."
fi
