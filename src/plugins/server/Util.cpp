//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "Util.h"
#include <sstream>

#ifdef __APPLE__
__thread char threadLocalBuffer[4096];
#else
thread_local char threadLocalBuffer[4096];
#endif

using namespace musik::core::sdk;

static size_t CopyString(const std::string& src, char* dst, size_t size) {
    size_t len = src.size() + 1; /* space for the null terminator */
    if (dst) {
        size_t copied = src.copy(dst, size - 1);
        dst[copied] = '\0';
        return copied + 1;
    }
    return len;
}

/* toHex, urlEncode, fromHex, urlDecode are stolen from here:
http://dlib.net/dlib/server/server_http.cpp.html */
static inline unsigned char toHex(unsigned char x) {
    return x + (x > 9 ? ('A' - 10) : '0');
}

static inline unsigned char fromHex(unsigned char ch) {
    if (ch <= '9' && ch >= '0') {
        ch -= '0';
    }
    else if (ch <= 'f' && ch >= 'a') {
        ch -= 'a' - 10;
    }
    else if (ch <= 'F' && ch >= 'A') {
        ch -= 'A' - 10;
    }
    else {
        ch = 0;
    }

    return ch;
}

std::string urlEncode(const std::string& s) {
    std::ostringstream os;

    for (std::string::const_iterator ci = s.begin(); ci != s.end(); ++ci) {
        if ((*ci >= 'a' && *ci <= 'z') ||
            (*ci >= 'A' && *ci <= 'Z') ||
            (*ci >= '0' && *ci <= '9'))
        { // allowed
            os << *ci;
        }
        else if (*ci == ' ') {
            os << '+';
        }
        else {
            os << '%' << toHex(*ci >> 4) << toHex(*ci % 16);
        }
    }

    return os.str();
}

std::string urlDecode(const std::string& str) {
    using namespace std;
    string result;
    string::size_type i;

    for (i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            result += ' ';
        }
        else if (str[i] == '%' && str.size() > i + 2) {
            const unsigned char ch1 = fromHex(str[i + 1]);
            const unsigned char ch2 = fromHex(str[i + 2]);
            const unsigned char ch = (ch1 << 4) | ch2;
            result += ch;
            i += 2;
        }
        else {
            result += str[i];
        }
    }

    return result;
}

IValue* CreateValue(
    const std::string& value, int64_t id, const std::string& type)
{
    struct Value : IValue {
        int64_t id;
        std::string value, type;

        Value(const std::string& value, int64_t id, const std::string& type) {
            this->value = value;
            this->id = id;
            this->type = type;
        }

        virtual int64_t GetId() { return id; }
        virtual Class GetClass() { return IResource::Class::Value; }
        virtual const char* GetType() { return type.c_str(); }
        virtual void Release() { delete this; }

        virtual size_t GetValue(char* dst, size_t size) {
            return CopyString(value, dst, size);
        }
    };

    return new Value(value, id, type);
}