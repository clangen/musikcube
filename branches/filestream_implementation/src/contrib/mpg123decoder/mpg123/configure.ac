dnl configure.ac: source for the configure script

dnl copyright by the mpg123 project - free software under the terms of the LGPL 2.1
dnl see COPYING and AUTHORS files in distribution or http://mpg123.org
dnl initially written by Nicholas J. Humfrey

dnl Require autoconf version >= 2.57
AC_PREREQ(2.57)


dnl ############# Initialisation

AC_INIT([mpg123], [1.5.1], [mpg123-devel@lists.sourceforge.net])
LIBMPG123_VERSION=7:0:7
AC_SUBST( LIBMPG123_VERSION )

AC_CONFIG_SRCDIR(src/mpg123.c)
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_SRCDIR(doc)
AC_CANONICAL_TARGET

dnl Version 1.7 of automake is recommended
AM_INIT_AUTOMAKE( 1.7 )
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
else
	AC_MSG_RESULT([no])
fi
if test "x$all_static" = xyes; then
	AC_MSG_WARN( Use -static in LDFLAGS for all-static linking! Your compiler may blow up on that -all-static. )
fi

AM_PROG_AS
AC_PROG_CC
AC_PROG_INSTALL

AC_C_CONST
AC_INLINE


dnl ############# Use Libtool for dynamic module loading

LTDLDIR=
OUTPUT_OBJ="module.\$(OBJEXT)"
dnl Enable building of the libltdl library
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
	if test "x$be_static" = "xyes" -o "x$with_cpu" = "xgeneric_float"; then
		modules=disabled
	else
		# Per default no modules in Windows...
		AC_CHECK_HEADERS([windows.h], modules=disabled, modules=enabled)
	fi
])

dnl We only want shared libraries by default
AC_DISABLE_STATIC
AC_ENABLE_SHARED

if test "$enable_shared" = no; then
	modules=disabled
	LT_LDFLAGS=
else
	AC_DEFINE(DYNAMIC_BUILD, 1, [ Define if building with dynamcally linked libmpg123])
fi

if test $modules = disabled
then
	echo "Modules disabled, not preparing libltdl"
else
LTDLDIR=libltdl
AC_LIBLTDL_INSTALLABLE
AC_DEFINE(HAVE_LTDL, 1, [ Define if LTDL library is available. ])

dnl Check for dlopen support
AC_LIBTOOL_DLOPEN

# Enable module support in source code
AC_DEFINE( USE_MODULES, 1, [Define if modules are enabled] )
fi

dnl Configure libtool

dnl Hacks for libtool 1.5 to avoid unnecessary checks, check for removal on newer libtool that is smarter
m4_defun([_LT_AC_LANG_CXX_CONFIG], [:])
m4_defun([_LT_AC_LANG_F77_CONFIG], [:])

AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

if test $modules = disabled
then
	echo "Modules still disabled..."
	LIBLTDL=
	LTDLINCL=
	MODULE_OBJ="legacy_module.\$(OBJEXT)"
else
	MODULE_OBJ="module.\$(OBJEXT)"
dnl Configure libltdl
AC_CONFIG_SUBDIRS(libltdl)
fi

AM_CONDITIONAL( [HAVE_MODULES], [test "x$modules" = xenabled] )

AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_SUBST(LTDLDIR)
AC_SUBST(MODULE_OBJ)
AC_SUBST(LT_LDFLAGS)

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

if test "$debugging" = enabled; then
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
                  AC_DEFINE(FIFO, 1, [ Define if FIFO support is enabled. ])
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
                  AC_DEFINE(IPV6, 1, [ Define if IPv6 support is enabled. ])
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
                  AC_DEFINE(network, 1, [ Define if network support is enabled. ])
                else
                  network="disabled"
                fi
              ],
              [
                network="auto"
              ]
)

AC_ARG_WITH([cpu], [
  --with-cpu=generic[_fpu]      Use generic processor code with floating point arithmetic
  --with-cpu=generic_float      A special build with generic fpu code that produces 32bit float output (experimental)
  --with-cpu=generic_nofpu      Use generic processor code with fixed point arithmetic (p.ex. ARM, experimental)
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
  --with-cpu=x86          Pack all x86 opts into one binary (excluding i486 and dithered i586)
  --with-cpu=x86_dither   Pack all x86 opts into one binary (excluding i486, including dither)
  --with-cpu=altivec      Use code optimized for Altivec processors (PowerPC G4 and G5)
]) 

AC_ARG_WITH([audio], [
  --with-audio=<list of modules>  Select a list (or only one) of audio output modules (comma or space separated list).
])
AC_ARG_WITH([default-audio], [
  --with-default-audio=aix        Use AIX as default audio output sub-system
  --with-default-audio=alib       Use Alib as default audio output sub-system (for HPUX)
  --with-default-audio=alsa       Use ALSA as default audio output sub-system
  --with-default-audio=dummy      Use dummy as default audio (when no sound card is available)
  --with-default-audio=esd        Use ESoundD as default audio output sub-system
  --with-default-audio=hp         Use HP as default audio output sub-system
  --with-default-audio=jack       Use JACK as default low-latency audio server
  --with-default-audio=macosx     Use Mac OS X as default audio output sub-system (CoreAudio)
  --with-default-audio=mint       Use MinT as default audio output sub-system (Atari)
  --with-default-audio=nas        Use NAS as default audio output (Network Audio System)
  --with-default-audio=os2        Use OS2 as default audio output sub-system
  --with-default-audio=oss        Use OSS as default audio output sub-system (/dev/dsp)
  --with-default-audio=portaudio  Use PortAudio as default audio output sub-system
  --with-default-audio=pulse      Use Pulse audio server as default audio output sub-system
  --with-default-audio=sdl        Use SDL as default audio output sub-system (Simple DirectMedia Layer)
  --with-default-audio=sgi        Use SGI as default audio output sub-system (IRIX)
  --with-default-audio=sun        Use Sun as default audio output sub-system (/dev/audio)
  --with-default-audio=arts       Use aRts as default audio output sub-system (KDE sound server)
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

dnl ############## Assembler, compiler properties

# based on posting from John Dalgliesh <johnd@defyne.org> on ffmpeg (LGPL) mailing list
# find if .align arg is power-of-two or not
asmalign_exp="unknown"
if test $asmalign_exp = "unknown"; then
	AC_MSG_CHECKING([if .align takes 2-exponent])
	asmalign_exp="no"
	echo '.align 3' > conftest.s
	if $CCAS -c -o conftest.o conftest.s; then
		asmalign_exp="yes"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	rm -f conftest.o conftest.s
fi
if test $asmalign_exp = "yes"; then
	AC_DEFINE(ASMALIGN_EXP, 1, [ Define if .align takes 3 for alignment of 2^3=8 bytes instead of 8. ])
fi

ccalign="unknown"
if test $ccalign = "unknown"; then
	AC_MSG_CHECKING([__attribute__((aligned(16)))])
	ccalign="no"
	echo '__attribute__((aligned(16))) float var;' > conftest.c
	if $CC -c -o conftest.o conftest.c; then
		ccalign="yes"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	rm -f conftest.o conftest.c
fi
if test $ccalign = "yes"; then
	AC_DEFINE(CCALIGN, 1, [ Define if __attribute__((aligned(16))) works ])
fi

dnl ############## Types

# Using the lower level macros instead of AC_TYPE_* for compatibility with not freshest autoconf.
AC_CHECK_TYPE(size_t, unsigned int)
AC_CHECK_TYPE(ssize_t, int)
AC_CHECK_TYPE(off_t,  long int)

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

AC_CHECK_FUNCS( mkfifo, [ have_mkfifo=yes ], [ have_mkfifo=no ] )

if test $fifo = "auto"; then
	if test $have_mkfifo = "yes"; then
		fifo=yes
		AC_DEFINE(FIFO, 1, [ Define if FIFO support is enabled. ])
	else
		fifo=no
	fi
fi
if test $fifo = "yes" && test $have_mkfifo = "no"; then
	AC_MSG_WARN( [ You forced FIFO code while I think there is no mkfifo() available! ] )
fi

AC_CHECK_FUNCS( getaddrinfo, [ have_ipv6=yes ], [ have_ipv6=no ] )
if test $ipv6 = "auto"; then
	if test $have_ipv6 = "yes"; then
		ipv6=yes
		AC_DEFINE(IPV6, 1, [ Define if IPV6 support is enabled. ])
	else
		ipv6=no
	fi
fi
if test $ipv6 = "yes" && test $have_ipv6 = "no"; then
	AC_MSG_WARN( [ You forced IPv6 code while I think there is no getaddrinfo() available! ] )
fi

dnl ############## Header and Library Checks

AC_HEADER_STDC
dnl Is it too paranoid to specifically check for stdint.h and limits.h?
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h sched.h sys/ioctl.h stdint.h limits.h inttypes.h sys/time.h sys/wait.h sys/resource.h signal.h])
# Headers for network (http) stuff
AC_CHECK_HEADERS([netdb.h sys/param.h sys/socket.h netinet/in.h arpa/inet.h])
if test "x$ac_cv_header_netdb_h" = "xyes"      &&
   test "x$ac_cv_header_sys_param_h" = "xyes"  &&
   test "x$ac_cv_header_sys_socket_h" = "xyes" &&
   test "x$ac_cv_header_netinet_in_h" = "xyes" &&
   test "x$ac_cv_header_arpa_inet_h"  = "xyes"; then
  have_network=yes
else
  have_network=no
fi

if test $network = "auto"; then
	if test $have_network = "yes"; then
		network=yes
		AC_DEFINE(NETWORK, 1, [ Define if network support is enabled. ])
	else
		network=no
	fi
fi
if test $network = "yes" && test $have_network = "no"; then
	AC_MSG_WARN( [ You forced network code while I think there is support missing! ] )
fi


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
#if test $ac_cv_header_sys_signal_h = yes; then
#	AC_CHECK_FUNCS( sigemptyset sigaddset sigprocmask sigaction )
#	if test $ac_cv_func_sigemptyset = yes &&
#	   test $ac_cv_func_sigaddset = yes &&
#	   test $ac_cv_func_sigprocmask = yes &&
#	   test $ac_cv_func_sigaction = yes; then
#	AC_DEFINE( 
#fi

dnl ############## Choose compiler flags and CPU

# do not assume gcc here, so no flags by default
ADD_CFLAGS=""
ADD_CPPFLAGS=""
LIBS="$LIBS"

AC_CHECK_HEADER([os2.h], [ADD_CPPFLAGS="$ADD_CPPFLAGS -DOS2"])

# If debugging is enabled then make warnings errors
if test "$debugging" = "enabled"; then
	ADD_CFLAGS="-g"
	# gcc specific...
	if test "$GCC" = "yes"; then
		ADD_CFLAGS="$ADD_CFLAGS -Wall -fno-strict-aliasing"
		if test "$nagging" = "enabled"; then
			ADD_CFLAGS="$ADD_CFLAGS -Werror -pedantic"
		fi
	fi
fi

case $target in
  arm-*-linux*)
    # check that... perhaps we are better off on arm with kernel math emulation
    cpu_type="generic_nofpu"
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
  *-*-linux*|*-*-kfreebsd*-gnu)
    cpu_type="generic_fpu"
  ;;
  i686-apple-darwin*)
    cpu_type="x86"
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
  i386-*-freebsd*)
    cpu_type="i386_fpu"
  ;;
  *-*-freebsd*)
    cpu_type="generic_fpu"
  ;;
  *-*-solaris*)
    cpu_type="generic_fpu"
    LIBS="$LIBS -lsocket -lnsl"
  ;;
  *-dec-osf*)
    cpu_type="generic_fpu"
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
  *-*-mingw32)
    LIBS="-lwsock32 $LIBS"
    buffer=disabled
    cpu_type="x86"
  ;;
	i386-*)
		AC_MSG_WARN([Unknown target operating system])
		cpu_type="i386"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i486-*)
		AC_MSG_WARN([Unknown target operating system])
		cpu_type="i486"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i586-*)
		AC_MSG_WARN([Unknown target operating system])
		cpu_type="i586"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
	i686-*)
		AC_MSG_WARN([Unknown target operating system])
		cpu_type="x86"
		buffer=disabled
		ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
	;;
  *)
  	AC_MSG_WARN([Unknown target operating system])
    cpu_type="generic_fpu"
    buffer=disabled
    ADD_CPPFLAGS="-DGENERIC $ADD_CPPFLAGS"
  ;;
esac


dnl Did user choose other CPU type ?
if test "x$with_cpu" != "x"; then
	cpu_type=$with_cpu
fi


s_gen="decode dct64"
s_altivec="decode_altivec dct64_altivec"
s_i386="decode_i386 dct64_i386"
s_i486="$s_i386 decode_i486 dct64_i486"
s_i586="$s_i386 decode_i586"
s_i586d="$s_i386 decode_i586_dither dnoise"
s_3dnow="$s_i386 decode_3dnow dct64_3dnow dct36_3dnow equalizer_3dnow"
s_3dnowext="decode_i386 dct64_3dnowext dct36_3dnowext dct64_mmx tabinit_mmx decode_3dnowext"
s_mmx="decode_i386 dct64_mmx tabinit_mmx decode_mmx"  # doesn't need dct64_i386
s_sse="decode_i386 dct64_mmx tabinit_mmx dct64_sse decode_sse"
s_multi="getcpuflags"

dnl CPU specific compiler flags and sources
case $cpu_type in
  generic)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FLOAT"
    more_sources="$s_gen"
  ;;
  generic_float)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DFLOATOUT -DREAL_IS_FLOAT"
    buffer=disabled
    more_sources="$s_gen"
  ;;
  generic_fpu)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FLOAT"
    more_sources="$s_gen"
  ;;
  generic_nofpu)
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_GENERIC -DREAL_IS_FIXED"
    more_sources="$s_gen"
  ;;
  altivec)
    ADD_CFLAGS="$ADD_CFLAGS -maltivec -faltivec"
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DREAL_IS_FLOAT -DOPT_ALTIVEC"
    more_sources="$s_altivec"
  ;;
  i386) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FLOAT"
    more_sources="$s_i386"
  ;;
  i386_fpu) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FLOAT"
    more_sources="$s_i386"
  ;;
  i386_nofpu) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I386 -DREAL_IS_FIXED"
    more_sources="$s_i386"
  ;;
  i486) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I486 -DREAL_IS_FLOAT"
    more_sources="$s_i486"
  ;;
  i586) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I586 -DREAL_IS_FLOAT"
    more_sources="$s_i586"
  ;;
  i586_dither) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_I586_DITHER -DREAL_IS_FLOAT"
    more_sources="$s_i586d"
  ;;
  3dnow)
    # legacy 3dnow had the 3dnow optional... keeping that
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_I586 -DOPT_3DNOW -DREAL_IS_FLOAT"
    more_sources="$s_i586 $s_3dnow $s_multi "
  ;;
  3dnowext_alone) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_3DNOWEXT -DREAL_IS_FLOAT"
    more_sources="$s_3dnowext"
  ;;
  3dnowext) 
    AC_MSG_WARN([Combining 3DNowExt decoder with 3DNow for flexible rate, use 3dnowext_alone if you do not want this.])
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_3DNOW -DOPT_3DNOWEXT -DREAL_IS_FLOAT"
    more_sources="$s_3dnowext $s_3dnow $s_multi"
  ;;
  mmx_alone) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MMX -DREAL_IS_FLOAT"
    more_sources="$s_mmx"
  ;;
  mmx)
    AC_MSG_WARN([Combining MMX decoder with i586 for flexible rate, use mmx_alone if you do not want this.])
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_I586 -DOPT_MMX -DREAL_IS_FLOAT"
    more_sources="$s_mmx $s_i586 $s_multi"
  ;;
  sse) 
    AC_MSG_WARN([Combining SSE decoder with i586 for flexible rate, use sse_alone if you do not want this.])
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_I586 -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_sse $s_i586 $s_multi"
  ;;
  sse_alone) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_sse"
  ;;
  x86) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_GENERIC -DOPT_I386 -DOPT_I586 -DOPT_MMX -DOPT_3DNOW -DOPT_3DNOWEXT -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_gen $s_i386 $s_i586 $s_mmx $s_3dnow $s_3dnowext $s_sse $s_multi"
 ;;
  x86_dither) 
    ADD_CPPFLAGS="$ADD_CPPFLAGS -DOPT_MULTI -DOPT_GENERIC -DOPT_I386 -DOPT_I586 -DOPT_I586_DITHER -DOPT_MMX -DOPT_3DNOW -DOPT_3DNOWEXT -DOPT_SSE -DREAL_IS_FLOAT"
    more_sources="$s_gen $s_i386 $s_i586 $s_i586d $s_mmx $s_3dnow $s_3dnowext $s_sse $s_multi"
  ;;
  *)
  	AC_MSG_ERROR([Unknown CPU type '$cpu_type'])
  ;;
esac

for i in $more_sources
do
	DECODER_OBJ="$DECODER_OBJ $i.\$(OBJEXT)"
	echo $DECODER_LOBJ | grep -q $i.lo || DECODER_LOBJ="$DECODER_LOBJ $i.lo"
done

AC_SUBST( DECODER_OBJ )
AC_SUBST( DECODER_LOBJ )


dnl ############## Output module choice

# The full list of supported modules to check, first come, first serve.
check_modules="alsa oss coreaudio sun win32 esd jack portaudio pulse sdl nas arts dummy"
# The final list.
output_modules=

check_forced=no
check_failed=no
if test "x$with_audio" != "x"; then
	check_modules="`echo $with_audio|tr , ' '` dummy"
	echo "Limiting outputs to build according to your preference: $check_modules"
	check_forced=yes
fi

if test $cpu_type = "generic_float"; then
	check_modules="dummy"
	AC_MSG_WARN( [Forcing dummy output (decoding to wav/stdout only) with float samples, as the audio device system cannot handle floats.] )
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
			case $target in
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
				if test $modules = disabled; then
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
		os2)
			UNSUPPORTED_AUDIO=yes
			OS2_LIBS="-los2me -lsocket"
			AC_CHECK_HEADERS([os2.h os2me.h])
			if test "x${ac_cv_header_os2_h}" = xyes \
			   -a   "x${ac_cv_header_os2me_h}" = xyes
			then
				output_modules="$output_modules os2"
				HAVE_OS2=yes
			else
				check_failed=yes
			fi
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
#for i in alsa oss coreaudio sun win32 esd jack portaudio pulse sdl nas aix alib arts hp os2 sgi mint dummy; do echo $i; done |
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
#for i in alsa coreaudio esd jack nas oss portaudio pulse sdl sun win32 aix alib arts hp os2 sgi mint; do echo $i; done |
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


AC_SUBST( OUTPUT_OBJ )
AC_SUBST( OUTPUT_MOD )
AC_SUBST( OUTPUT_CFLAGS )
AC_SUBST( OUTPUT_LIBS )
AC_SUBST( OUTPUT_LDFLAGS )

dnl ############## Compiler Optimizations

CFLAGS="$ADD_CFLAGS $CFLAGS"
if test $buffer = disabled; then
	ADD_CPPFLAGS="$ADD_CPPFLAGS -DNOXFERMEM"
fi
CPPFLAGS="$ADD_CPPFLAGS $CPPFLAGS"

# None chosen?
if test "x$with_optimization" = "x"; then
	if test "$debugging" = "enabled"; then
		with_optimization="0"
	else
		# enable (gcc specific) default opts only with gcc
		if test "$GCC" = yes; then
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

dnl ############## Seektable size

if test "x$with_seektable" = "x"; then
	seektable=1000
else
	seektable=$with_seektable
fi

AC_DEFINE_UNQUOTED(INDEX_SIZE, $seektable, [size of the frame index seek table])


dnl ############## Final Output

AC_CONFIG_FILES([
	Makefile
	libmpg123.pc
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
  CPU Optimisation......... $cpu_type
  Compiler Optimization ... $with_optimization
  Gapless Support ......... $gapless
  Debugging ............... $debugging
  Seek table size ......... $seektable
  FIFO support ............ $fifo
  Buffer .................. $buffer
  Network (http streams) .. $network
  IPv6 (getaddrinfo) ...... $ipv6
"
if test "$modules" = enabled; then
	echo "  Modules Directory ....... $pkglibdir"
fi
echo "  Checked audio modules ... $check_modules
  Detected audio support ..$output_modules
  Default output module ... $default_output_module
"
if test $cpu_type = "generic_float"; then
	echo "You chose the generic build with 32bit float output.
That means output via -s, -O or -w to a file/pipe and no buffer.
"
fi
if test "$modules" = disabled; then
echo "The _single_ active output module is being statically linked in.
"
else
	if test "$HAVE_WIN32" = yes; then
	echo "WARNING: The output modules most likely won't work for you under Windows (MinGW32)..."
	echo "You can try to hack our libtool stuff into making it happen -- and share your knowledge with the mpg123 team."
	echo
	fi
fi
if test $with_optimization = "0"; then
	echo "No optimization flags chosen, make sure you have something basic in your CFLAGS at least...
"
fi
if test "$debugging" = "enabled"; then
	echo "CFLAGS='$CFLAGS'"
	echo "LIBS='$LIBS'"
else
    echo "Next type 'make' and then 'make install'."
fi


