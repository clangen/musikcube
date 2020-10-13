//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
#include <string.h>
#include <memory>

#ifdef WIN32
#include <Windows.h>
#endif

namespace musik { namespace core { namespace sdk { namespace str {

    #ifdef WIN32
        template <typename String=std::string>
        static inline String u16to8(const wchar_t* utf16) {
            if (!utf16) return "";
            int size = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, 0, 0, 0, 0);
            if (size <= 0) return "";
            char* buffer = new char[size];
            WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buffer, size, 0, 0);
            std::string utf8str(buffer);
            delete[] buffer;
            return utf8str;
        }

        template <typename Wstring=std::wstring>
        static inline Wstring u8to16(const char* utf8) {
            int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
            if (size <= 0) return L"";
            wchar_t* buffer = new wchar_t[size];
            MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, size);
            std::wstring utf16fn(buffer);
            delete[] buffer;
            return utf16fn;
        }
    #endif

    template<typename... Args>
    static std::string format(const std::string& format, Args ... args) {
        /* https://stackoverflow.com/a/26221725 */
        size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; /* extra space for '\0' */
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        return std::string(buf.get(), buf.get() + size - 1); /* omit the '\0' */
    }

} } } }
