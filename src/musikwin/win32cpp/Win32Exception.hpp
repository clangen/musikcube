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
 * @file Win32Exception.hpp
 * @brief Exception that captures a Win32 system error code.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Win32Exception
 * records the GetLastError() code at construction time and renders it to a
 * human readable string using FormatMessage().
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Exception.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief An Exception that represents a Win32 error. */
class Win32Exception : public Exception
{
public: // ctors
    /** @brief Captures the current GetLastError() value. */
    /*ctor*/    Win32Exception()
    {
        this->errorCode = ::GetLastError();
    }

    /** @brief Constructs the exception from an explicit error code.
     *  @param errorCode the Win32 error code to store */
    /*ctor*/    Win32Exception(DWORD errorCode)
    {
        this->errorCode = errorCode;
    }

public: // methods
    /** @brief Formats the stored error code into a readable string.
     *  @return pointer to the formatted error message
     *  @note The returned buffer is statically allocated. */
    virtual const char* Message()
    {
        static char resultPtr[4096];
        resultPtr[0] = 0;
        //
        ::FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            Application::Instance(),
            this->errorCode,
            NULL,
            resultPtr,
            4096,
            NULL);
        //
        return resultPtr;
    }

    /** @brief Returns the raw Win32 error code.
     *  @return the stored error code */
    DWORD ErrorCode()
    {
        return this->errorCode;
    }

private: // instance data
    DWORD errorCode; /**< the captured Win32 error code */
};

//////////////////////////////////////////////////////////////////////////////

} // namespace win32cpp

//////////////////////////////////////////////////////////////////////////////
