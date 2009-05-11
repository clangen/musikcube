//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include <core/Common.h>
#include <core/config_filesystem.h>
#include <utf8/utf8.h>

//////////////////////////////////////////
///\brief
///Get the directory where plugin-dlls are located.
///
///\returns
///String with the directory
///
///\see
///<GetApplicationDirectory>
//////////////////////////////////////////
utfstring musik::core::GetPluginDirectory(){
    utfstring sDirectory(GetApplicationDirectory());
    sDirectory.append(UTF("plugins/"));
    return sDirectory;
}

//////////////////////////////////////////
///\brief
///Get the directory where http files are located.
///
///\returns
///String with the directory
///
///\see
///<GetApplicationDirectory>
//////////////////////////////////////////
utfstring musik::core::GetWebfilesDirectory(){
    utfstring sDirectory(GetApplicationDirectory());
    sDirectory.append(UTF("htdocs/"));
    return sDirectory;
}


//////////////////////////////////////////
///\brief
///Get path to where the application is located.
///
///\returns
///String with the path
//////////////////////////////////////////
utfstring musik::core::GetApplicationDirectory(){

    utfstring sDirectory;

    #ifdef WIN32
        utfchar szPath[2048];
        int iStrLength    = GetModuleFileName(NULL,szPath,2048);
        if(iStrLength!=0 && iStrLength<2048 ){
            sDirectory.assign( GetPath(utfstring(szPath)) );
        }
    #endif

    return sDirectory;
}

utfstring musik::core::GetDataDirectory(){
    utfstring directory;

    #ifdef WIN32
        DWORD iBufferSize    = GetEnvironmentVariable(UTF("APPDATA"), 0, 0);

        utfchar *sBuffer    = new utfchar[iBufferSize+2];
        GetEnvironmentVariable(UTF("APPDATA"), sBuffer, iBufferSize);
        directory.assign(sBuffer);

        delete [] sBuffer;
    #endif

    directory.append(UTF("/mC2/"));

    // Create folder if it does not exist
    boost::filesystem::utfpath oFolder(directory);
    if(!boost::filesystem::exists(oFolder)){
        boost::filesystem::create_directories(oFolder);
    }

    return directory;
}


//////////////////////////////////////////
///\brief
///Find out the full path to a file.
///
///\param sFile
///File to get the full path to.
///
///\returns
///String with path.
//////////////////////////////////////////
utfstring musik::core::GetPath(const utfstring &sFile){
    utfstring sPath;

    utfchar szPath[2048];
    utfchar *szFile=NULL;
    int iStrLength    = GetFullPathName(sFile.c_str(),2048,szPath,&szFile);
    if(iStrLength!=0 && iStrLength<2048 ){
        sPath.assign(szPath);
        if(szFile!=0){
            utfstring sTheFile(szFile);
            sPath.assign(sPath.substr(0,iStrLength-sTheFile.length()));
        }
    }else{
        sPath.assign(sFile);
    }

    return sPath;
}

//////////////////////////////////////////
///\brief
///Convert std::wstring to std::string
///
///\param sString
///std::wstring
///
///\returns
///Converted string
//////////////////////////////////////////
std::string musik::core::ConvertUTF8(const std::wstring &sString){
    std::string sUTF8;
    utf8::utf16to8(sString.begin(),sString.end(),std::back_inserter(sUTF8));
    return sUTF8;
}

//////////////////////////////////////////
///\brief
///Convert std::string to std::wstring
///
///\param sString
///std::string
///
///\returns
///Converted string
//////////////////////////////////////////
std::wstring musik::core::ConvertUTF16(const std::string &sString){
    std::wstring sUTF16;
    utf8::utf8to16(sString.begin(),sString.end(),std::back_inserter(sUTF16));
    return sUTF16;
}

//////////////////////////////////////////
///\brief
///Convert char* to std::wstring
///
///\param sString
///std::wstring
///
///\returns
///Converted string
//////////////////////////////////////////
std::wstring musik::core::ConvertUTF16(const char *string){
    std::string sString(string);
    std::wstring sUTF16;
    utf8::utf8to16(sString.begin(),sString.end(),std::back_inserter(sUTF16));
    return sUTF16;
}

UINT64 musik::core::Checksum(char *data,unsigned int bytes){
    UINT64 sum(0);
    for(unsigned int i(0);i<bytes;++i){
        char theChar    = *(data+i);
        sum += (UINT64)theChar;
    }
    return sum;
}




