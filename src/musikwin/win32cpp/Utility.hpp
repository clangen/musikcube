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
 * @file Utility.hpp
 * @brief Small free-standing helper functions for string conversion.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Provides
 * escaping, hexadecimal parsing and ANSI/wide string conversion helpers
 * used across the library.
 */

#pragma once

#include <win32cpp/Types.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief Escapes Win32 control characters (e.g. \\t, \\n) in a string.
 *  @param string the input string to escape
 *  @return a copy of the input with special characters escaped
 *  @note The escaped form is what is stored in INI-style config files. */
uistring Escape(uistring string);

/** @brief Parses a hexadecimal string into an integer.
 *  @param value the hexadecimal string to parse (e.g. "FF")
 *  @return the parsed value, or 0 on failure */
int HexToInt(const uichar* value);

/** @brief Converts a narrow ANSI string to a wide string.
 *  @param str the narrow string to widen
 *  @return the wide-string equivalent */
uistring WidenString(const char* str);

/** @brief Converts a wide string to a narrow ANSI string.
 *  @param str the wide string to shrink
 *  @return the narrow-string equivalent */
std::string ShrinkString(const uistring& str);

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
