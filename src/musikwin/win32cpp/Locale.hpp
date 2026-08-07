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
 * @file Locale.hpp
 * @brief Application localization / translation support.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Locale loads a
 * locale-specific translation table (stored via Config) and date/time
 * format settings, and exposes a process-wide singleton used by the _(),
 * _TT() and _TTP() translation macros.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Config.hpp>
#include <map>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Locale
//////////////////////////////////////////////////////////////////////////////

/** @brief Map of locale names to their display name. */
typedef std::map<uistring, uistring> LocaleList;
/** @brief Map of untranslated strings to translated strings. */
typedef std::map<uistring, uistring> LocaleTranslationMap;

/** @brief Loads and provides localized strings and formats.
 *  @details A singleton accessed through Instance(). Translation strings
 *           are stored in an INI file inside the locale directory, and
 *           date/time formats are loaded for the active locale ID. */
class Locale {
private:
    Config          config;         /**< INI reader used for translations */
    uistring        localeDirectory; /**< directory holding locale files */
    uistring        localeName;      /**< name of the active locale */
    WORD            localeID;        /**< Win32 language identifier */
    uistring        dateFormat;      /**< localized date format string */
    uistring        timeFormat;      /**< localized time format string */

    LocaleTranslationMap
                    translationMap; /**< cached string translations */

public:
    /** @brief Loads the translation tables for the given locale.
     *  @param localeName name of the locale to load
     *  @return true if the locale was loaded successfully */
    bool            LoadConfig(const uistring& localeName);
    /** @brief Sets the directory that contains locale files.
     *  @param dirName path of the locale directory */
    void            SetLocaleDirectory(const uistring& dirName);
    /** @brief Returns the list of available locales.
     *  @return map of locale name to display name */
    LocaleList      EnumLocales(void);
    /** @brief Translates the given string.
     *  @param original the string to translate
     *  @return the translated string (or the original if not found) */
    uistring        Translate(const uistring& original);
    /** @brief Translates the given C string.
     *  @param original the string to translate
     *  @return the translated string (or the original if not found) */
    uistring        Translate(const uichar* original);
    /** @brief Returns the name of the active locale.
     *  @return the locale name */
    uistring        LocaleName(void) const { return this->localeName; }
    /** @brief Returns the Win32 language identifier of the locale.
     *  @return the language ID */
    WORD            LangID(void) const { return this->localeID; }
    /** @brief Returns the localized date format.
     *  @return the date format string */
    uistring        DateFormat(void) const { return this->dateFormat; }
    /** @brief Returns the localized time format.
     *  @return the time format string */
    uistring        TimeFormat(void) const { return this->timeFormat; }
    /** @brief Determines whether the OS supports this locale.
     *  @return TRUE if the locale has system support */
    BOOL            SystemSupport(void) const;

    /** @brief Returns the process-wide Locale singleton.
     *  @return pointer to the single Locale instance
     *  @note Constructed lazily on first use (Meyers singleton). */
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

    /** @brief Creates an empty Locale. */
    /*ctor*/        Locale();
    /** @brief Creates a Locale for the given directory and locale name.
     *  @param dirName the directory containing locale files
     *  @param locale the name of the locale to activate */
    /*ctor*/        Locale(const uistring& dirName, const uistring& locale);
    /** @brief Destroys the Locale. */
    /*dtor*/        ~Locale();
};

#define _(ORIGINALTEXT) (win32cpp::Locale::Instance()->Translate(ORIGINALTEXT).c_str())
#define _TT(ORIGINALTEXT) (win32cpp::Locale::Instance()->Translate(_T(ORIGINALTEXT)))
#define _TTP(ORIGINALTEXT) (win32cpp::Locale::Instance()->Translate(_T(ORIGINALTEXT)).c_str())


//////////////////////////////////////////////////////////////////////////////

}
