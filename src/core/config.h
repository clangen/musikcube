//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#include <string>
#include <core/sdk/config.h>
#include <utf8/utf8.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

typedef UINT64 DBID;
typedef UINT64 VERSION;
typedef UINT64 DBTIME;

#ifdef WIN32
	#define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))
#else
	#define CopyFloat(dst, src, num) memmove(dst, src, (num) * sizeof(float))
#endif

inline std::wstring u8to16(const std::string& u8) {
    std::wstring result;
    utf8::utf8to16(u8.begin(), u8.end(), std::back_inserter(result));
    return result;
}

inline std::string u16to8(const std::wstring& u16) {
    std::string result;
    utf8::utf16to8(u16.begin(), u16.end(), std::back_inserter(result));
    return result;
}

inline static int u8len(const std::string& str) {
    try {
        return utf8::distance(str.begin(), str.end());
    }
    catch (...) {
        return str.length();
    }
}

inline static std::string u8substr(const std::string& in, int offset, int len) {
    std::string::const_iterator begin = in.begin() + offset;
    std::string::const_iterator it = begin;

    int count = 0;
    while (count < len && it != in.end()) {
        utf8::unchecked::next(it);
        ++count;
    }

    return std::string(begin, it);
}