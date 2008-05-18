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
#include <win32cpp/Config.hpp>
#include <boost/scoped_ptr.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

Config::Config()
{
    ;
}

Config::Config(const uistring &fileName) : iniFileName(fileName)
{
    ;
}

Config::~Config()
{
    ;
}

void Config::SetSection(const uistring &newSection)
{
    this->currentSection = newSection;
}

void Config::SetFileName(const uistring& fileName)
{
    this->iniFileName = fileName;
}

BOOL Config::SectionExists(const uistring& section)
{
    DWORD bufferLen = 255, retLen;
    boost::scoped_ptr<TCHAR> buffer(new TCHAR[bufferLen]);

    // get section keys
    retLen = ::GetPrivateProfileSection(
        section.c_str(),
        buffer.get(),
        bufferLen, 
        this->iniFileName.c_str());

    return (retLen > 0);
}

uistring Config::Value(const uistring &key)
{
    uistring value;		
    DWORD bufferLen = 255, retLen;
    boost::scoped_ptr<TCHAR> buffer(new TCHAR[bufferLen]);

    // get value
    retLen = ::GetPrivateProfileString(
        this->currentSection.c_str(), 
        key.c_str(), 
        NULL, 
        buffer.get(), 
        bufferLen, 
        this->iniFileName.c_str());

    // if size exceeds the limit, realloc and read again
    while(retLen == bufferLen-1)
    {
        bufferLen += bufferLen;
        buffer.reset(new TCHAR[bufferLen]);

        retLen = ::GetPrivateProfileString(
            this->currentSection.c_str(), 
            key.c_str(), 
            NULL, 
            buffer.get(), 
            bufferLen, 
            this->iniFileName.c_str());
    }

    if (retLen)
    {
        value = uistring(buffer.get());
    }

    return value;
}

BOOL Config::SetValue(const uistring &key, const uistring &value)
{
    // check if section name is empty
    if(this->currentSection.empty()) 
        throw Exception("Please provide a section!");

    // write value to ini file
    BOOL ret = ::WritePrivateProfileString(
        this->currentSection.c_str(), 
        key.c_str(), 
        value.c_str(), 
        this->iniFileName.c_str());

    // throw an exception if call was invalid
    if(!ret) 
        throw Win32Exception();

    return ret;
}

ConfigSectionList Config::Sections()
{
    ConfigSectionList list;
    DWORD bufferLen = 255, retLen;
    boost::scoped_ptr<TCHAR> buffer(new TCHAR[bufferLen]);

    // get string with section names
    retLen = ::GetPrivateProfileSectionNames(
        buffer.get(),
        bufferLen, 
        this->iniFileName.c_str());

    // if string length exceeds the limit, realloc with bigger size and read again
    while(retLen == bufferLen-1)
    {
        bufferLen += bufferLen;
        buffer.reset(new TCHAR[bufferLen]);

        retLen = ::GetPrivateProfileSectionNames(
            buffer.get(),
            bufferLen, 
            this->iniFileName.c_str());
    }

    if (retLen)
    {
        boost::scoped_ptr<TCHAR> buffer2(new TCHAR[retLen]);
        
        // split up string with section names   
        for(DWORD lastIndex = 0, i = 0; i < retLen; i++) 
        {
            if(buffer.get()[i] == 0) {
                // copy section name to separate buffer
                ::_tcsncpy_s(
                    buffer2.get(),
                    i + 1,
                    &buffer.get()[lastIndex],
                    _TRUNCATE);

                // push section name into list
                list.push_back(buffer2.get());

                // save last index to get next element
                lastIndex = i + 1;

                // the end of the list is terminated with \0\0
                if(buffer.get()[i+1] == 0) break;
            }
        }
    }

    return list;
}
