//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: mC2, win32cpp
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

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///A memory device context. MemoryDC is used internally by various classes,
///including Window, for flicker free drawing.
///
///When performing graphics operations on a Window's DC, changes are drawn
///directly to screen; this leads to flickering. MemoryDC creates an offscreen
///buffer that "accumulates" changes, which are copied to a desination DC
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
public:     /*ctor*/    MemoryDC(HDC hdc, const RECT& rect);
public:     /*dtor*/    ~MemoryDC();

public:     operator    HDC();

private:    HBITMAP     memoryBitmap;
private:    HDC         memoryDC, screenDC;
private:    HANDLE      oldObject;
private:    RECT        clientRect;
private:    bool        rectIsValid;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp