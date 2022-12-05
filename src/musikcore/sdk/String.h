//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
#include <vector>
#include <algorithm>

#ifdef WIN32
#include <Windows.h>
#endif

namespace musik { namespace core { namespace sdk { namespace str {

    #ifdef WIN32
        template <typename String = std::string>
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

        template <typename Wstring = std::wstring>
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
    static std::string Format(const std::string& format, Args ... args) {
        /* https://stackoverflow.com/a/26221725 */
        size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; /* extra space for '\0' */
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        return std::string(buf.get(), buf.get() + size - 1); /* omit the '\0' */
    }

    static inline bool IsSpace(const char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f';
    }

    template<typename String = std::string>
    static String Trim(const String& str) {
        if (str.size()) {
            int length = (size_t)str.size();
            int start = 0, end = length;
            int i = 0;
            while (i < length) {
                if (!IsSpace(str[i])) {
                    break;
                }
                ++start;
                ++i;
            }
            i = end - 1;
            while (i >= 0) {
                if (!IsSpace(str[i])) {
                    break;
                }
                --i;
            }
            if (end >= start) {
                std::string result = str.substr((size_t)start, (size_t)end - start);
                return result;
            }
        }
        return str;
    }

    template<typename String = std::string>
    static String Trim(const char* str) {
        return Trim(String(str));
    }

    template<typename String = std::string, typename Result = std::vector<String>>
    static Result Split(const String& in, const String& delim) {
        Result result;
        size_t last = 0, next = 0;
        while ((next = in.find(delim, last)) != std::string::npos) {
            result.push_back(std::move(Trim(in.substr(last, next - last))));
            last = next + 1;
        }
        result.push_back(std::move(Trim(in.substr(last))));
        return result;
    }

    template<typename String = std::string, typename Result = std::vector<String>>
    static Result Split(const String& in, const char* delim) {
        return Split(in, String(delim));
    }

    template<typename String = std::string>
    static void ReplaceAll(String& input, const String& find, const String& replace) {
        size_t pos = input.find(find);
        while (pos != std::string::npos) {
            input.replace(pos, find.size(), replace);
            pos = input.find(find, pos + replace.size());
        }
    }

    template<typename String = std::string>
    static void ReplaceAll(String& input, const char* find, const char* replace) {
        ReplaceAll(input, String(find), String(replace));
    }

    template<typename String = std::string>
    static String ReplaceAllCopy(const String& input, const String& find, const String& replace) {
        String copy = input;
        ReplaceAll(copy, find, replace);
        return copy;
    }

    template<typename String = std::string>
    static String ReplaceAllCopy(const String& input, const char* find, const char* replace) {
        return ReplaceAllCopy(input, String(find), String(replace));
    }

    template<typename String = std::string>
    static String ToLowerCopy(const String& input) {
        String copy = input;
        std::transform(copy.begin(), copy.end(), copy.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return copy;
    }

} } } }
