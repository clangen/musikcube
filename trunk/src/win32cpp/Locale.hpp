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

#pragma once

#include <win32cpp/Win32Config.hpp>          // Must be first!
#include <win32cpp/Types.hpp>   
#include <win32cpp/Config.hpp>
#include <map>   

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Locale
//////////////////////////////////////////////////////////////////////////////

typedef std::map<uistring, uistring> LocaleList;
typedef std::map<uistring, uistring> LocaleTranslationMap;

class Locale {
private:
    Config          config;
    uistring        localeDirectory;
    uistring        localeName;
    WORD            localeID;
    uistring        dateFormat;
    uistring        timeFormat;
    
    LocaleTranslationMap
                    translationMap;

public:
    bool            LoadConfig(const uistring& localeName);
    void            SetLocaleDirectory(const uistring& dirName);
    LocaleList      EnumLocales(void);
    uistring        Translate(const uistring& original);
    uistring        LocaleName(void) const { return this->localeName; }
    WORD            LangID(void) const { return this->localeID; }
    uistring        DateFormat(void) const { return this->dateFormat; }
    uistring        TimeFormat(void) const { return this->timeFormat; }
    BOOL            SystemSupport(void) const;


    static Locale*  Instance()
    {
        // singleton implementation (scott meyers variant)
        // due to the static initialization the compiler is creating code
        // with an atexit()-registration of a function which is responsible
        // for a guaranteed destruction of the singleton object when
        // the program exists (resource leaks can be avoided by this method)
        static Locale singletonInstance;

        // we return the pointer so we can pass the Locale instance
        // around without the need to forbid instance-based 
        // (copy-)construction
        return &singletonInstance;
    }

    /*ctor*/        Locale();
    /*ctor*/        Locale(const uistring& dirName, const uistring& locale);
    /*dtor*/        ~Locale();
};

#define _(ORIGINALTEXT) (win32cpp::Locale::Instance()->Translate(ORIGINALTEXT))


//////////////////////////////////////////////////////////////////////////////

}