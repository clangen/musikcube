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

#pragma once

/// @file MemoryDC.h
/// @brief Off-screen GDI device context for flicker-free drawing.
/// @details Part of the win32cpp library (Casey Langen). MemoryDC accumulates
/// drawing operations in an off-screen bitmap that is copied to a target HDC
/// on destruction, eliminating flicker. Used by the GDI visualizer plugin.

#include <Windows.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///A memory device context. MemoryDC is used internally by various classes,
///including Window, for flicker free drawing.
///
///When performing graphics operations on a Window's DC, changes are drawn
///directly to screen; this leads to flickering. MemoryDC creates an offscreen
///buffer that "accumulates" changes, which are copied to a destination DC
///when all drawing has finished.
///
///MemoryDC is constructed with an HDC and a Rect as parameters, and provides
///an implicit HDC cast operator that returns a handle to the offscreen buffer.
///This means that regular Win32 drawing routines, such as DrawLine, FillRect,
///etc, can transparently use a MemoryDC as if it were a regular HDC.
///
///When a MemoryDC's destructor is called the contents of the offscren buffer
///are automatically copied to to the HDC it was constructed with, resulting
///in flicker-free drawing.
///
///\code
///PAINTSTRUCT paintStruct;
///HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
///{
///    MemoryDC memDC(hdc, paintStruct.rcPaint);
///
///    //...
///    //draw to memDC as if you were drawing to hdc
///    //...
///
///} // when the MemoryDC destructor is called, the contents will be copied to hdc
///::EndPaint(this->Handle(), &paintStruct);
///\endcode
///
///\see
///RedrawLock
class MemoryDC
{
public: // constructors, destructor
    /** @brief Creates an off-screen buffer for the given rect.
     *  @param hdc The target device context.
     *  @param rect The region to buffer. */
    /*ctor*/    MemoryDC(HDC hdc, const RECT& rect);
    /** @brief Copies the off-screen buffer to the target DC. */
    /*dtor*/    ~MemoryDC();

public: // operators
    /** @brief Returns a handle to the off-screen buffer.
     *  @return The memory HDC. */
    operator    HDC();

private: // instance data
    /** @brief The off-screen bitmap. */
    HBITMAP memoryBitmap;
    /** @brief Memory and screen device contexts. */
    HDC memoryDC, screenDC;
    /** @brief Previously selected object to restore. */
    HANDLE oldObject;
    /** @brief The buffered client rectangle. */
    RECT clientRect;
    /** @brief Whether the client rect is valid. */
    bool rectIsValid;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp