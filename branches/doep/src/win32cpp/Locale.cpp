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

#include <pch.hpp>
#include <win32cpp/Locale.hpp>
#include <win32cpp/Win32Exception.hpp>
#include <win32cpp/Utility.hpp>
#include "boost/filesystem/operations.hpp"

#include <iostream>

namespace fs = boost::filesystem;

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

Locale::Locale()
{
    // set default language identifier 
    // 0x0409 = English (United States) / en-US / Codepage 1252
    this->localeID = 0x0409;
}

Locale::Locale(const uistring& dirName, const uistring& locale)
{
    Locale();

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
    uistring path = this->localeDirectory + _T("\\" + localeName + L".ini");

    if(!fs::exists(path)) 
    {
        return false;
    }

    this->config.SetFileName(path);

    // read locale configuration
    this->config.SetSection(_T("config"));

    // read name (for combo box displaying the languages)
    this->localeName = this->config.Value(_T("name"));
    if(this->localeName.empty()) 
    {
        return false;
    }

    // read locale identifier, necessary for e.g. DateTime
    // (http://msdn.microsoft.com/en-us/library/ms776260(VS.85).aspx)
    // example: 0x0407 for german
    uistring temp = this->config.Value(_T("locale_identifier"));
    int id = HexToInt(temp.c_str());
    if(id != 0) 
    {
        this->localeID = static_cast<WORD>(id);    
    }

    // read format strings for date/time formatting
    this->dateFormat = this->config.Value(_T("locale_date"));
    this->timeFormat = this->config.Value(_T("locale_time"));
       
    // generate translation map
    for(int i = 1; ; i++) 
    {
        // convert int to tchar
        TCHAR section[8];
        _stprintf_s(section, _T("%d"), i);

        if(this->config.SectionExists(uistring(section))) 
        {
            this->config.SetSection(section);

            // put translation into map
            uistring 
                original    = config.Value(_T("original")),
                translated  = config.Value(_T("translated"));

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

LocaleList Locale::EnumLocales(void)
{
    uistring path = this->localeDirectory;

    // check if path is valid
    if(!fs::exists(path) && fs::is_directory(path)) 
    {
        throw new Exception("Could not enumerate. Directory with locales is invalid.");
    }
    
    // iterate through locale directory and collect available locales
    LocaleList localeList;
    
    // convert directory to boost path
    fs::path localePath(ShrinkString(path), fs::native); 

    // now iterate...
    fs::directory_iterator iEnd;
    for(fs::directory_iterator iDir(localePath); iDir != iEnd; ++iDir)
    {
        // lame conversion =(
        std::string dirEntryA = iDir->path().leaf();
        uistring dirEntryW = WidenString(dirEntryA.c_str());

        // read name of config
        Config entryConfig(path + _T("\\") + dirEntryW);
        if(entryConfig.SectionExists(_T("config"))) 
        {
            entryConfig.SetSection(_T("config"));
            localeList[dirEntryW.c_str()] = entryConfig.Value(_T("name"));
        }
    }

    return localeList;
}


BOOL Locale::SystemSupport() const
{
    return ::IsValidLocale(this->localeID, LCID_INSTALLED);
}
