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

//////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

    #define WIN32_LEAN_AND_MEAN
    #define WINVER 0x0501
    #define _WIN32_WINNT 0x0501
    #include <windows.h>
    #include <tchar.h>

    #ifndef MUSIKCOREIMPORT
        //#define DLLEXPORT __declspec( dllexport )
        #define DLLEXPORT
    #else
        #define DLLEXPORT __declspec( dllimport )
    #endif
    typedef unsigned __int64 UINT64;

    #define STDCALL(fp) __stdcall fp

#else
    #include <cassert>

    typedef unsigned long long UINT64;
    typedef long long __int64;		//TODO: Is this necessary?

    #define STDCALL(fp) fp __attribute__((stdcall))
    #define _ASSERT assert
	#define TEXT(s) s		//There's no TEXT() on linux. This makes the current uses of it compile for me... Jooles

#endif  // WIN32

////////////////////////////////

typedef unsigned int DBINT;
typedef UINT64 VERSION;
typedef unsigned int DBTIME;

////////////////////////////////

#include <string>


#ifdef _MSC_VER

    //////////////////////////////////////////
    ///\brief
    /// Some methods need to know if the utfchar is a wide char.
    /// We define UTF_WIDECHAR for this
    //////////////////////////////////////////
    #define UTF_WIDECHAR

    //////////////////////////////////////////
    ///\brief
    ///The UTF macro is the text macro when writing texts in the code that
    ///should be utf encoded.
    ///
    ///\remarks
    ///We thought that the _T was a little bit too microsoft specific.
    //////////////////////////////////////////
    #define UTF(x)  L"x"

    //////////////////////////////////////////
    ///\brief
    ///utfchar is either a char or a utfchar depending on system.
    //////////////////////////////////////////
    typedef wchar_t utfchar;

    //////////////////////////////////////////
    ///\brief
    ///utfstringstream is the stringstream for utfchar & utfstring
    //////////////////////////////////////////
/*    namespace std
    {
        typedef wstringstream   utfstringstream;
        typedef wostringstream  utfostringstream;
        typedef wistringstream  utfistringstream;
    }
*/
#else
    #undef UTF_WIDECHAR

    //////////////////////////////////////////
    ///\brief
    ///utfchar is either a char or a utfchar depending on system.
    //////////////////////////////////////////
    typedef char utfchar;

    //////////////////////////////////////////
    ///\brief
    ///The UTF macro is the text macro when writing texts in the code that
    ///should be utf encoded.
    ///
    ///\remarks
    ///We thought that the _T was a little bit too microsoft specific.
    //////////////////////////////////////////
    #define UTF(x)  x

    //////////////////////////////////////////
    ///\brief
    ///utfstringstream is the stringstream for utfchar & utfstring
    //////////////////////////////////////////
    namespace std
    {
        typedef stringstream   utfstringstream;
        typedef ostringstream  utfostringstream;
        typedef istringstream  utfistringstream;
    }

#endif

typedef std::basic_string<utfchar> utfstring;

//////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
	#define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))
#else
	#define CopyFloat(dst, src, num) memmove(dst, src, (num) * sizeof(float))
#endif

//////////////////////////////////////////////////////////////////////////////
