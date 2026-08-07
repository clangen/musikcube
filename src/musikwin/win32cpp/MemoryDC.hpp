//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
//
// Sources and Binaries of: win32cpp
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright 
//      notice, this list of conditions and the following disclaimer in the 
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may 
//      be used to endorse or promote products derived from this software 
//      without specific prior written permission. 
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE. 
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @file MemoryDC.hpp
 * @brief Off-screen memory device context for flicker-free drawing.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. MemoryDC creates
 * an off-screen bitmap and device context; drawing happens on the buffer
 * and its contents are blitted to the destination DC when the object is
 * destroyed, eliminating flicker.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A memory device context.
 *  @details Used internally by various classes, including Window, for
 *           flicker free drawing. When performing graphics operations on a
 *           Window's DC, changes are drawn directly to the screen, which
 *           leads to flickering. MemoryDC creates an offscreen buffer that
 *           "accumulates" changes, which are copied to a destination DC
 *           when all drawing has finished.
 *
 *           MemoryDC is constructed with an HDC and a Rect as parameters,
 *           and provides an implicit HDC cast operator that returns a
 *           handle to the offscreen buffer. This means that regular Win32
 *           drawing routines, such as DrawLine, FillRect, etc, can
 *           transparently use a MemoryDC as if it were a regular HDC.
 *
 *           When a MemoryDC's destructor is called the contents of the
 *           offscreen buffer are automatically copied to the HDC it was
 *           constructed with, resulting in flicker-free drawing.
 *  @code
 *  PAINTSTRUCT paintStruct;
 *  HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
 *  {
 *      MemoryDC memDC(hdc, paintStruct.rcPaint);
 *      // draw to memDC as if you were drawing to hdc
 *  } // contents are copied to hdc on destruction
 *  ::EndPaint(this->Handle(), &paintStruct);
 *  @endcode
 *  @see RedrawLock */
class MemoryDC
{
public: // constructors, destructor
    /** @brief Creates an off-screen buffer for the given DC region.
     *  @param hdc the destination device context
     *  @param rect the region to buffer */
    /*ctor*/    MemoryDC(HDC hdc, const RECT& rect);
    /** @brief Blits the off-screen buffer to the destination DC. */
    /*dtor*/    ~MemoryDC();

public: // operators
    /** @brief Returns a handle to the off-screen buffer. */
    operator    HDC();

private: // instance data
    HBITMAP memoryBitmap; /**< the off-screen bitmap */
    HDC memoryDC, screenDC; /**< off-screen and destination DCs */
    HANDLE oldObject;       /**< the previously selected bitmap */
    RECT clientRect;        /**< the buffered region */
    bool rectIsValid;       /**< whether the rect is valid */
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
