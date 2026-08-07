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
 * @file Config.hpp
 * @brief INI-file based configuration reader/writer.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Config wraps the
 * Win32 GetPrivateProfileString / WritePrivateProfileString APIs to read
 * and write key/value pairs organised into named sections of an INI file.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <vector>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////////////

/** @brief List of section names found in a configuration file. */
typedef std::vector<uistring> ConfigSectionList;

/** @brief Reads and writes INI-style configuration files.
 *  @details Backed by the Win32 profile APIs. A Config object points at a
 *           single INI file and tracks a "current section"; Value()/SetValue()
 *           operate on that section by default. */
class Config {
private:
    uistring    currentSection;   /**< section used when none is specified */
    uistring    iniFileName;      /**< path of the INI file being managed */
public:
    /** @brief Creates a Config with no file associated. */
    /*ctor*/    Config();
    /** @brief Creates a Config pointing at the given INI file.
     *  @param fileName path of the INI file to read/write */
    /*ctor*/    Config(const uistring& fileName);
    /** @brief Destroys the Config object. */
    /*dtor*/    ~Config();
    /** @brief Selects the section used by subsequent calls.
     *  @param newSection name of the section to select */
    void        SetSection(const uistring& newSection);
    /** @brief Changes the INI file this Config operates on.
     *  @param fileName path of the new INI file */
    void        SetFileName(const uistring& fileName);
    /** @brief Returns the value of a key in the current section.
     *  @param key the key to look up
     *  @return the stored value, or an empty string if not found */
    uistring    Value(const uistring& key);
    /** @brief Writes a key/value pair to the current section.
     *  @param key the key to write
     *  @param value the value to store
     *  @return TRUE on success */
    BOOL        SetValue(const uistring& key, const uistring& value);
    /** @brief Determines whether a section exists in the file.
     *  @param section the section name to test
     *  @return TRUE if the section exists */
    BOOL        SectionExists(const uistring& section);
    /** @brief Returns the list of all section names in the file.
     *  @return the list of section names */
    ConfigSectionList   Sections();
};

//////////////////////////////////////////////////////////////////////////////

}
