//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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

#include <pch.hpp>
#include <win32cpp/Utility.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

uistring win32cpp::Escape(uistring string){
    boost::algorithm::replace_all(string,_T("&"),_T("&&"));
    return string;
}

uistring win32cpp::WidenString(const char* sourceBuffer)
{
    int length = (int) strlen(sourceBuffer) + 1;
    size_t convertedChars = 0;
    boost::scoped_array<uichar> targetBuffer(new uichar[length]);

    mbstowcs_s(
        &convertedChars,
        targetBuffer.get(),
        length,
        sourceBuffer,
        length);

    return uistring(targetBuffer.get());
}

std::string win32cpp::ShrinkString(const uistring& sourceString)
{
    int length = (int) sourceString.length() + 1;
    boost::scoped_ptr<char> targetBuffer(new char[length]);
    size_t convertedChars = 0;

    wcstombs_s(
        &convertedChars,
        targetBuffer.get(),
        length,
        sourceString.c_str(),
        length);

    return std::string(targetBuffer.get());
}

int win32cpp::HexToInt(const uichar* value)
{
    struct CHexMap
    {
        TCHAR chr;
        int value;
    };

    const int HexMapL = 16;
    CHexMap HexMap[HexMapL] =
    {
        {'0', 0}, {'1', 1},
        {'2', 2}, {'3', 3},
        {'4', 4}, {'5', 5},
        {'6', 6}, {'7', 7},
        {'8', 8}, {'9', 9},
        {'A', 10}, {'B', 11},
        {'C', 12}, {'D', 13},
        {'E', 14}, {'F', 15}
    };

    size_t length = _tcslen(value) + 1;
    TCHAR *mstr = _tcsdup(value);
    _tcsupr_s(mstr, length);
    TCHAR *s = mstr;
    int result = 0;
    if (*s == '0' && *(s + 1) == 'X') s += 2;
    bool firsttime = true;
    while (*s != '\0')
    {
        bool found = false;
        for (int i = 0; i < HexMapL; i++)
        {
            if (*s == HexMap[i].chr)
            {
                if (!firsttime) result <<= 4;
                result |= HexMap[i].value;
                found = true;
                break;
            }
        }
        if (!found) break;
        s++;
        firsttime = false;
    }
    free(mstr);
    return result;
}


//////////////////////////////////////////////////////////////////////////////


