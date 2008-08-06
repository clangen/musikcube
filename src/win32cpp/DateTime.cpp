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
#include <win32cpp/DateTime.hpp>
#include <win32cpp/Win32Exception.hpp>
#include <win32cpp/Utility.hpp>
#include <boost/regex.hpp>
#include <sstream>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

DateTime::DateTime() : locale(NULL)
{
    // use current local time
    this->SetLocaltime();
}

DateTime::DateTime(Locale* useLocale) : locale(useLocale)
{
    // use current local time
    ::GetLocalTime(&this->curDateTime);
}

DateTime::DateTime(Locale* useLocale, const SYSTEMTIME& dateTime) : 
    locale(useLocale), 
    curDateTime(dateTime)
{
    // use given time
    ;
}
  
DateTime::~DateTime()
{
}
    
void DateTime::SetSystemtime(void)
{
    ::GetSystemTime(&this->curDateTime);
}

void DateTime::FromSystemtime(const SYSTEMTIME& dateTime)
{
    this->curDateTime = dateTime;
}

void DateTime::SetLocaltime(void)
{
    ::GetLocalTime(&this->curDateTime);
}

bool DateTime::FromSQLDateTime(const uistring& sqlDate)
{
    // match YYYY-MM-DD HH:MM:SS
    // HH:MM:SS is optional
    boost::cmatch matches;
    boost::regex e("(\\d{4})-(\\d{2})-(\\d{2})(?: (\\d{2}):(\\d{2}):(\\d{2}))?");

    std::string str = ShrinkString(sqlDate);
    const char* cstr = str.c_str();

    if(boost::regex_match(cstr, matches, e))
    {
        for(unsigned int i = 1; i <= 6; i++)
        {
            int c;
            if(matches[i].matched)
            {
                std::string match(matches[i].first, matches[i].second);
                std::stringstream mstream(match);
                mstream >> c;
            }
            else
            {
                c = 0;
            }

            switch(i)
            {
            case 1: this->curDateTime.wYear   = c; break;
            case 2: this->curDateTime.wMonth  = c; break;
            case 3: this->curDateTime.wDay    = c; break;
            case 4: this->curDateTime.wHour   = c; break;
            case 5: this->curDateTime.wMinute = c; break;
            case 6: this->curDateTime.wSecond = c; break;
            }
        }
        
        this->curDateTime.wMilliseconds = 0;

        return true;
    }

    return false;
}

void DateTime::Set(int year = -1, int month = -1, int dow = -1, int day = -1, int hour = -1, int minute = -1, int second = -1, int millisecond = -1)
{
    // use negative numbers if you don't want to set an element
    if(year >= 0) this->curDateTime.wYear = year;
    if(month >= 0) this->curDateTime.wMonth = month;
    if(dow >= 0) this->curDateTime.wDayOfWeek = dow;
    if(day >= 0) this->curDateTime.wDay = day;
    if(hour >= 0) this->curDateTime.wHour = hour;
    if(minute >= 0) this->curDateTime.wMinute = minute;
    if(second >= 0) this->curDateTime.wSecond = second;
    if(millisecond >= 0) this->curDateTime.wMilliseconds = millisecond; 
}

uistring DateTime::FormatDate(const uistring& format, DWORD flags = 0)
{
    TCHAR formatted[64];
    LPCTSTR fstr = NULL;
    WORD langid = (this->locale != NULL) 
        ? (this->locale->LangID())
        : LOCALE_USER_DEFAULT;

    if(!format.empty()) fstr = format.c_str();

    int ret = ::GetDateFormat(
        MAKELCID(langid, SORT_DEFAULT),
        flags,
        &this->curDateTime,
        fstr,
        formatted,
        64
    );

    if(!ret) {
        throw Win32Exception();
    }

    return uistring(formatted);
}

uistring DateTime::FormatTime(const uistring& format, DWORD flags = 0)
{
    TCHAR formatted[64];
    LPCTSTR fstr = NULL;
    WORD langid = (this->locale != NULL) 
        ? (this->locale->LangID())
        : LOCALE_USER_DEFAULT;

    if(!format.empty()) fstr = format.c_str();

    int ret = ::GetTimeFormat(
        MAKELCID(langid, SORT_DEFAULT),
        flags,
        &this->curDateTime,
        fstr,
        formatted,
        64
    );

    if(!ret) {
        throw Win32Exception();
    }

    return uistring(formatted);
}

uistring DateTime::Date()
{
    // is a locale set?
    if(this->locale) 
    {
        // get format string for dates
        uistring fmtDate = this->locale->DateFormat();
        if(fmtDate.empty()) 
        {
            // if the date format is empty, use the locale-specific
            return this->FormatDate(_T(""), DATE_SHORTDATE);
        } 
        else 
        {
            // if the date format is given, use it
            return this->FormatDate(fmtDate);
        }
    } 
    else
    {
        // if no locale is set, use the user/system-specific
        // (which is ALWAYS installed and supported)
        return this->FormatDate(_T(""), DATE_SHORTDATE);
    }
}

uistring DateTime::Time()
{
    // is a locale set?
    if(this->locale) 
    {
        // get format string for times
        uistring fmtTime = this->locale->TimeFormat();
        if(fmtTime.empty()) 
        {
            // if the time format is empty, use the locale-specific
            return this->FormatTime(_T(""));
        } 
        else 
        {
            // if the time format is given, use it
            return this->FormatTime(fmtTime);
        }
    } 
    else
    {
        // if no locale is set, use the user/system-specific
        // (which is ALWAYS installed and supported)
        return this->FormatTime(_T(""));
    }
}

uistring DateTime::MonthString()
{
    // if a locale is given and installed, return specified by locale
    if(this->locale && this->locale->SystemSupport()) 
    {
        return this->FormatDate(_T("MMMM"));
    } 

    // otherwise, use the standard names, passing them through the translation map of the locale
    // if a locale is not installed in the NLS-subsystem of windows
    switch(this->curDateTime.wMonth)
    {
    case 1: return _(_T("January"));
    case 2: return _(_T("February"));
    case 3: return _(_T("March"));
    case 4: return _(_T("April"));
    case 5: return _(_T("May"));
    case 6: return _(_T("June"));
    case 7: return _(_T("July"));
    case 8: return _(_T("August"));
    case 9: return _(_T("September"));
    case 10: return _(_T("October"));
    case 11: return _(_T("November"));
    case 12: return _(_T("December"));
    }
    
    return _T("(null)");
}

uistring DateTime::DayOfWeekString()
{
    // if a locale is given and installed, return specified by locale
    if(this->locale && this->locale->SystemSupport()) 
    {
        return this->FormatDate(_T("dddd"));
    } 

    // otherwise, use the standard names, passing them through the translation map of the locale
    // if a locale is not installed in the NLS-subsystem of windows
    switch(this->curDateTime.wDayOfWeek)
    {
    case 0: return _(_T("Sunday"));
    case 1: return _(_T("Monday"));
    case 2: return _(_T("Tuesday"));
    case 3: return _(_T("Wednesday"));
    case 4: return _(_T("Thursday"));
    case 5: return _(_T("Friday"));
    case 6: return _(_T("Saturday"));
    }
    
    return _T("(null)");
}

