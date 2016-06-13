//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #define WINVER 0x0502
    #define _WIN32_WINNT 0x0502
    #define NOMINMAX

    #include <windows.h>
    #include <tchar.h>

    #define sleep_ms(x) Sleep(x)

    typedef __int64 int64;
    typedef unsigned __int64 uint64;

    #define STDCALL(fp) (__stdcall* fp)()
#else
    #include <cassert>
    #include <unistd.h>

    #define sleep_ms(x) usleep(x * 1000)

    typedef __uint64_t uint64;
    typedef __int64_t int64;

    #define STDCALL(fp) (* fp)()
    #define _ASSERT assert
#endif

#include <string>

/* a super small collection of inlined tools that most plugins will
need to use. these should be header only, and have no dependencies
other than STL */
namespace musik {
    namespace sdk {
        inline bool endsWith(const std::string& s, const std::string& suffix) {
            return
                s.size() >= suffix.size() &&
                s.rfind(suffix) == (s.size() - suffix.size());
        }
    }
}
