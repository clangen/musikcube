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

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Locale.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// DateTime
//////////////////////////////////////////////////////////////////////////////

class DateTime {
private:
    SYSTEMTIME      curDateTime;
    Locale*         locale;
public:
    int             Year()              const { return this->curDateTime.wYear; }
    int             Month()             const { return this->curDateTime.wMonth; }
    int             DayOfWeek()         const { return this->curDateTime.wDayOfWeek; }
    int             Day()               const { return this->curDateTime.wDay; }
    int             Hour()              const { return this->curDateTime.wHour; }
    int             Minute()            const { return this->curDateTime.wMinute; }
    int             Second()            const { return this->curDateTime.wSecond; }
    int             Millisecond()       const { return this->curDateTime.wMilliseconds; }

    uistring        MonthString();
    uistring        DayOfWeekString();
    
    uistring        Date();
    uistring        Time();
    ULONG           Timestamp()         const;

    uistring        FormatDate(const uistring& format, DWORD flags);
    uistring        FormatTime(const uistring& format, DWORD flags);

    const PSYSTEMTIME
                    Win32Systemtime();

    void            Set(int year, int month, int dow, int day, int hour, int minute, int second, int millisecond);
    void            SetSystemtime(void);
    void            SetLocaltime(void);

    void            FromSystemtime(const SYSTEMTIME& dateTime);
    bool            FromSQLDateTime(const uistring& sqlDate);

    /* ctor */      DateTime();
    /* ctor */      DateTime(Locale* useLocale);
    /* ctor */      DateTime(Locale* useLocale, const SYSTEMTIME& dateTime);
    /* dtor */      ~DateTime();
};


//////////////////////////////////////////////////////////////////////////////

}
