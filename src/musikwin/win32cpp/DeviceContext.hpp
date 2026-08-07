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
 * @file DeviceContext.hpp
 * @brief RAII wrapper around a Win32 window device context.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. DeviceContext
 * obtains a device context for a window with GetDC() at construction and
 * releases it with ReleaseDC() at destruction.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief RAII wrapper for a device context acquired from a window.
 *  @details Calls GetDC() on construction and ReleaseDC() on destruction,
 *           so the caller never has to manually release the HDC.
 *  @see Window, MemoryDC */
class DeviceContext
{
public: // constructors, methods
    /** @brief Acquires a device context for the given window.
     *  @param hwnd the window whose device context to obtain */
    /*ctor*/    DeviceContext(HWND hwnd);
    /** @brief Releases the device context. */
    /*dtor*/    ~DeviceContext();

    /** @brief Returns the underlying HDC. */
    operator HDC() { return this->hdc; }

private: // instance data
    HDC hdc;   /**< the acquired device context */
    HWND hwnd; /**< the window the DC was acquired from */
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
