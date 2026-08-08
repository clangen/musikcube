//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Andr� W�sten
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

/**
 * @file DateTime.hpp
 * @brief Date/time value wrapper around Win32 SYSTEMTIME.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. DateTime wraps a
 * Win32 SYSTEMTIME structure and adds accessor methods, localized
 * formatting (through Locale), and SQL/Systemtime conversion helpers.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Locale.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// DateTime
//////////////////////////////////////////////////////////////////////////////

/** @brief Represents a date and time, backed by Win32 SYSTEMTIME.
 *  @details Provides field accessors (year, month, day, time), localized
 *           string formatting via the associated Locale, and conversion
 *           between SYSTEMTIME, local time and SQL datetime strings. */
class DateTime {
private:
    SYSTEMTIME      curDateTime; /**< the wrapped SYSTEMTIME value */
    Locale*         locale;      /**< locale used for localized output */
public:
    /** @brief Returns the year component.
     *  @return the year (e.g. 2016) */
    int             Year()              const { return this->curDateTime.wYear; }
    /** @brief Returns the month component.
     *  @return the month (1-12) */
    int             Month()             const { return this->curDateTime.wMonth; }
    /** @brief Returns the day of the week.
     *  @return the day of week (0 = Sunday, 6 = Saturday) */
    int             DayOfWeek()         const { return this->curDateTime.wDayOfWeek; }
    /** @brief Returns the day of the month.
     *  @return the day (1-31) */
    int             Day()               const { return this->curDateTime.wDay; }
    /** @brief Returns the hour component.
     *  @return the hour (0-23) */
    int             Hour()              const { return this->curDateTime.wHour; }
    /** @brief Returns the minute component.
     *  @return the minute (0-59) */
    int             Minute()            const { return this->curDateTime.wMinute; }
    /** @brief Returns the second component.
     *  @return the second (0-59) */
    int             Second()            const { return this->curDateTime.wSecond; }
    /** @brief Returns the millisecond component.
     *  @return the millisecond (0-999) */
    int             Millisecond()       const { return this->curDateTime.wMilliseconds; }

    /** @brief Returns the localized name of the month.
     *  @return the month name string */
    uistring        MonthString();
    /** @brief Returns the localized name of the day of the week.
     *  @return the day of week name string */
    uistring        DayOfWeekString();

    /** @brief Returns the localized date string.
     *  @return the formatted date */
    uistring        Date();
    /** @brief Returns the localized time string.
     *  @return the formatted time */
    uistring        Time();
    /** @brief Returns the time as a Win32 timestamp.
     *  @return the FILETIME-style timestamp */
    ULONG           Timestamp()         const;

    /** @brief Formats the date using the given format string.
     *  @param format the GetDateFormat format string
     *  @param flags additional GetDateFormat flags
     *  @return the formatted date string */
    uistring        FormatDate(const uistring& format, DWORD flags);
    /** @brief Formats the time using the given format string.
     *  @param format the GetTimeFormat format string
     *  @param flags additional GetTimeFormat flags
     *  @return the formatted time string */
    uistring        FormatTime(const uistring& format, DWORD flags);

    /** @brief Returns a pointer to the wrapped SYSTEMTIME.
     *  @return pointer to the internal SYSTEMTIME */
    const PSYSTEMTIME
                    Win32Systemtime();

    /** @brief Sets the date and time components explicitly.
     *  @param year the year
     *  @param month the month (1-12)
     *  @param dow the day of the week
     *  @param day the day of the month
     *  @param hour the hour
     *  @param minute the minute
     *  @param second the second
     *  @param millisecond the millisecond */
    void            Set(int year, int month, int dow, int day, int hour, int minute, int second, int millisecond);
    /** @brief Populates the value with the current system time. */
    void            SetSystemtime(void);
    /** @brief Populates the value with the current local time. */
    void            SetLocaltime(void);

    /** @brief Copies the value from a SYSTEMTIME.
     *  @param dateTime the SYSTEMTIME to copy */
    void            FromSystemtime(const SYSTEMTIME& dateTime);
    /** @brief Parses the value from a SQL datetime string.
     *  @param sqlDate the SQL datetime string to parse
     *  @return true if parsing succeeded */
    bool            FromSQLDateTime(const uistring& sqlDate);

    /** @brief Constructs a DateTime for the current system time. */
    /* ctor */      DateTime();
    /** @brief Constructs a DateTime for the current system time.
     *  @param useLocale the locale used for formatted output */
    /* ctor */      DateTime(Locale* useLocale);
    /** @brief Constructs a DateTime from a SYSTEMTIME.
     *  @param useLocale the locale used for formatted output
     *  @param dateTime the SYSTEMTIME to wrap */
    /* ctor */      DateTime(Locale* useLocale, const SYSTEMTIME& dateTime);
    /** @brief Destroys the DateTime. */
    /* dtor */      ~DateTime();
};


//////////////////////////////////////////////////////////////////////////////

}
