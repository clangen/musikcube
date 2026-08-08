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
 * @file Exception.hpp
 * @brief Base exception types used throughout win32cpp.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Exception is the
 * common base class for every exception thrown by the library; it stores a
 * human readable message and exposes it through Message().
 */

#pragma once

#include <string>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief Base class for all exceptions thrown by win32cpp. */
class Exception
{
public: // constructors
    /** @brief Constructs an exception with an empty message. */
    /*ctor*/            Exception();
    /** @brief Constructs an exception with the given message.
     *  @param message the human readable error message */
    /*ctor*/            Exception(const char* message);
    /** @brief Destroys the exception. */
    /*dtor*/ virtual    ~Exception();

public: // methods
    /** @brief Returns the exception's error message.
     *  @return pointer to a null-terminated message string */
    virtual const char* Message();

private: // instance data
    std::string         message; /**< the stored error message */
};

//////////////////////////////////////////////////////////////////////////////

/** @brief Thrown when a method is not yet implemented. */
class NotImplementedException: public Exception { };

//////////////////////////////////////////////////////////////////////////////

} // namespace win32cpp
