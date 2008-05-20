//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, André Wösten
//
// Sources and Binaries of: mC2, win32cpp
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

#include "pch.hpp"
#include <win32cpp/Locale.hpp>
#include "boost/filesystem/operations.hpp"

#include <iostream>

namespace fs = boost::filesystem;

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

Locale::Locale()
{
    ;
}

Locale::Locale(const uistring& dirName, const uistring& locale)
{
    this->SetLocaleDirectory(dirName);
    if(!this->LoadConfig(locale)) 
    {
        throw Exception("Cannot load locale config!");
    }
}

Locale::~Locale()
{
    ;
}


uistring Locale::Translate(const uistring& original)
{
    if(this->translationMap.find(original) != this->translationMap.end()) 
    {
        return this->translationMap[original];
    }

    return original;    
}

bool Locale::LoadConfig(const uistring& localeName)
{
    uistring path = this->localeDirectory + L"\\" + localeName + L".ini";

    if(!fs::exists(path)) 
    {
        return false;
    }

    config.SetFileName(path);

    // generate translation map
    for(int i = 1; ; i++) 
    {
        // convert int to tchar
        TCHAR section[8];
        _stprintf_s(section, L"%d", i);

        if(config.SectionExists(uistring(section))) 
        {
            config.SetSection(section);

            // put translation into map
            uistring 
                original    = config.Value(L"original"),
                translated  = config.Value(L"translated");

            this->translationMap[original] = translated;
        }
        else
        {
            // break if next section has not been found
            break;
        }
    }

    return true;
}

void Locale::SetLocaleDirectory(const uistring &dirName)
{
    TCHAR localeDir[MAX_PATH];

    // get absolute path
    if(!::GetFullPathName(dirName.c_str(), MAX_PATH, localeDir, NULL)) 
    {
        throw Win32Exception();
    }

    // check if path exists
    if(!fs::exists(localeDir)) 
    {
        throw Exception("Cannot find directory with locales!");
    }

    this->localeDirectory = uistring(localeDir);
}
