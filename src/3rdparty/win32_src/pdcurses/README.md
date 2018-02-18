PDCurses for Win32a
==================

This directory contains PDCurses source code files specific to Win32
graphics mode (Win95 and all subsequent forks of Windows).

Building
--------

   (Note that the following is almost identical to the instructions
   for the Win32 console flavor of PDCurses.)

- Choose the appropriate makefile for your compiler:

        bccwin32.mak  - Borland C++ 4.0.2+
        dmcwin32.mak  - Digital Mars
        gccwin32.mak  - Cygnus GNU Compiler
        lccwin32.mak  - LCC-Win32
        mingwin32.mak - MinGW
        vcwin32.mak   - Microsoft Visual C++ 2.0+ or later & Intel(R) compiler
        wccwin32.mak  - Watcom 10.6+ or OpenWATCOM

- Optionally, you can build in a different directory than the platform
  directory by setting PDCURSES_SRCDIR to point to the directory where
  you unpacked PDCurses, and changing to your target directory:

        set PDCURSES_SRCDIR=c:\pdcurses

  This won't work with the LCC or Digital Mars makefiles, nor will the
  options described below.

- Build it:

        make -f makefilename

  (For Watcom, use "wmake" instead of "make"; for MSVC, "nmake".) You'll
  get the libraries (pdcurses.lib or .a, depending on your compiler; and
  panel.lib or .a), the demos (*.exe), and a lot of object files. Note
  that the panel library is just a copy of the main library, provided
  for convenience; both panel and curses functions are in the main
  library.

  You can also give the optional parameter "WIDE=Y", to build the
  library with wide-character (Unicode) support:

        make -f mingwin32.mak WIDE=Y

  When built this way, the library is not compatible with Windows 9x,
  unless you also link with the Microsoft Layer for Unicode (not
  tested).

  For the Intel(R) compiler,  use vcwin32.mak and add ICC=Y.

  By default,  vcwin32.mak results in 64-bit code for both VC and Intel(R).
  Add IX86=Y to generate 32-bit code.  (Other builds are 32-bit only.)

  Another option, "UTF8=Y", makes PDCurses ignore the system locale, and
  treat all narrow-character strings as UTF-8. This option has no effect
  unless WIDE=Y is also set. Use it to get around the poor support for
  UTF-8 in the Win32 console:

        make -f mingwin32.mak WIDE=Y UTF8=Y

  You can also use the optional parameter "DLL=Y" with Visual C++,
  MinGW or Cygwin, to build the library as a DLL:

        nmake -f vcwin32.mak WIDE=Y DLL=Y

  When you build the library as a Windows DLL, you must always define
  PDC_DLL_BUILD when linking against it. (Or, if you only want to use
  the DLL, you could add this definition to your curses.h.)

   If cross-compiling from Linux with MinGW,  ensure that _linux is defined:

        make -f mingwin32.mak _linux=1 [WIDE=Y UTF8=Y DLL=Y]

Distribution Status
-------------------

The files in this directory are released to the Public Domain.

Acknowledgements
----------------

Based heavily on the Win32 console flavor of PDCurses.
