//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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

#include "pch.hpp"
#include <win32cpp/MemoryDC.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// MemoryDC
//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param hdc
///The destination HDC. The final contents of the memory buffer will
///be copied here upon destruction.
///
///\param rect
///The Rectangle that represents the drawable area of the HDC.
/*ctor*/    MemoryDC::MemoryDC(HDC hdc, const RECT& rect)
: rectIsValid(false)
, screenDC(hdc)
, memoryBitmap(NULL)
, memoryDC(NULL)
, clientRect(rect)
{
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    this->memoryDC = ::CreateCompatibleDC(this->screenDC);
    //
    this->memoryBitmap = ::CreateCompatibleBitmap(this->screenDC, width, height);
    this->oldObject = ::SelectObject(this->memoryDC, this->memoryBitmap);

    this->rectIsValid = ((width > 0) && (height > 0));
}

/*dtor*/    MemoryDC::~MemoryDC()
{
    if (this->rectIsValid)
    {
        ::BitBlt(
            this->screenDC,
            this->clientRect.left,
            this->clientRect.top,
            this->clientRect.right - this->clientRect.left,
            this->clientRect.bottom - this->clientRect.top,
            this->memoryDC,
            0,
            0,
            SRCCOPY);
    }

    ::SelectObject(this->memoryDC, this->oldObject);

    ::DeleteObject(this->memoryBitmap);
    ::DeleteObject(this->memoryDC);
}

///\brief
///Returns a handle to the offscreen device context.
MemoryDC::operator HDC()
{
    return this->memoryDC;
}