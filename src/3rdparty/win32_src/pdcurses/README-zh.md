用于 WinGUI 的 PDCurses
==================

本目录包含特定于 Win32 图形模式（Win95 及所有后续 Windows 分支）的 PDCurses 源代码文件。

构建
--------

   （注意：以下内容与 Win32 控制台版本的 PDCurses 的说明几乎相同。）

- 为你的编译器选择合适的 makefile：

        Makefile.bcc  - Borland C++ 4.0.2+
        Makefile.dmc  - Digital Mars
        Makefile.lcc  - LCC-Win32
        Makefile.mng  - MinGW、Cygnus GNU Compiler
        Makefile.vc   - Microsoft Visual C++ 2.0+ 或更高版本以及 Intel(R) 编译器
        Makefile.wcc  - Watcom 10.6+ 或 OpenWATCOM

- 可选地，你可以通过设置 PDCURSES_SRCDIR 指向你解压 PDCurses 的目录，并在目标目录中构建，而不是在平台目录中构建：

        set PDCURSES_SRCDIR=c:\pdcurses

  这不适用于 LCC 或 Digital Mars 的 makefile，也不适用于下面描述的选项。

- 构建它：

        make -f makefilename

  （对于 Watcom 使用 "wmake" 代替 "make"；对于 MSVC 使用 "nmake"。）你将得到库（pdcurses.lib 或 .a，取决于你的编译器；以及 panel.lib 或 .a）、演示程序（*.exe）和大量目标文件。注意，panel 库只是主库的副本，为方便而提供；panel 和 curses 函数都在主库中。

  你还可以传入可选参数 "WIDE=Y" 来构建带宽字符（Unicode）支持的库：

        make -f Makefile.mng WIDE=Y

  以这种方式构建时，库与 Windows 9x 不兼容，除非你还链接 Microsoft Unicode 层（未经测试）。

  对于 Intel(R) 编译器，使用 Makefile.vc 并添加 ICC=Y。

  默认情况下，Makefile.vc 对 VC 和 Intel(R) 都会生成 64 位代码。添加 IX86=Y 以生成 32 位代码。（其他构建仅支持 32 位。）

  另一个选项 "UTF8=Y" 使 PDCurses 忽略系统区域设置，并将所有窄字符串视为 UTF-8。除非同时设置 WIDE=Y，否则此选项无效。这最初是为了解决 Win32 控制台对 UTF-8 支持不佳的问题：

        make -f Makefile.mng WIDE=Y UTF8=Y

  WinGUI 没有 Win32 控制台版本那样的限制，但 UTF-8 和非 UTF-8 版本仍然可用。无论如何，这意味着如果你用任何配置构建了 Win32 控制台 PDCurses DLL，你就可以构建一个匹配的 WinGUI DLL，并仅通过替换 DLL 在控制台或 GUI PDCurses 之间切换。

  你还可以对 Visual C++、MinGW 或 Cygwin 使用可选参数 "DLL=Y" 来将库构建为 DLL：

        nmake -f Makefile.vc WIDE=Y DLL=Y

  当你将库构建为 Windows DLL 时，链接时必须始终定义 PDC_DLL_BUILD。（或者，如果你只想使用 DLL，可以将此定义添加到你的 curses.h 中。）

  如果从 Linux 交叉编译，添加参数 `_w64=1` 以生成 64 位代码（默认为 32 位）。

        make -f Makefile.mng _w64=1 [WIDE=Y UTF8=Y DLL=Y]

分发状态
------------------

本目录中的文件以公有领域（Public Domain）发布。

致谢
---------------

主要基于 Chris Szurgot <szurgot[at]itribe.net> 的 Win32 控制台版本的 PDCurses，由 Bill Gray <pluto[at]projectpluto.com> 移植到 Win32 GUI。
