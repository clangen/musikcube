# win32cpp #

## Overview ##

win32cpp is a lightweight C++ abstraction of the Win32 API. It
  * is small and easy to use
  * is event driven
  * is designed for use with the [model-view-controller](http://en.wikipedia.org/wiki/Model_view_controller) design pattern
  * automatically double buffers all drawing (flicker free)
  * compiles cleanly
  * open source under the new bsd license
  * uses exception handling, and is exception safe

win32cpp depends on:
  * [the standard template library](http://www.cplusplus.com/reference/stl/)
  * [boost](http://www.boost.org/)
  * [sigslot](http://sigslot.sourceforge.net/)
  * [the platform sdk](http://www.microsoft.com/downloads/details.aspx?FamilyId=484269E2-3B89-47E3-8EB7-1F2BE6D7123A&displaylang=en)

## Documentation ##

We are in process of documenting the public API. [You can view the progress here](http://clangen.org/projects/win32cpp/api/).