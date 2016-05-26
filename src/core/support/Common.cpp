//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Daniel �nnerby
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
#include <core/support/Common.h>
#include <core/config.h>
#include <utf8/utf8.h>
#include <cstdlib>
#include <iostream>
#include <mach-o/dyld.h>

std::string musik::core::GetPluginDirectory() {
    std::string path(GetApplicationDirectory());
    path.append("/plugins/");
    return path;
}

std::string musik::core::GetApplicationDirectory() {
    std::string result;

    #ifdef WIN32
        wchar_t szPath[2048];
        int iStrLength = GetModuleFileName(NULL, szPath, 2048);
        if(iStrLength != 0 && iStrLength < 2048){
            result.assign(GetPath(u16to8(szPath).c_str()));
        }
    #else
      // char pathbuf[PATH_MAX + 1];
      // uint32_t bufsize = sizeof(pathbuf);
      // _NSGetExecutablePath(pathbuf, &bufsize);
      // result.assign(pathbuf);
      // std::cerr << result << std::endl;
      result = "/Users/clangen/src/musikcube/bin/";
    #endif

    return result;
}

std::string musik::core::GetDataDirectory() {
    std::string directory;

    #ifdef WIN32
        DWORD iBufferSize = GetEnvironmentVariable(_T("APPDATA"), 0, 0);
        wchar_t *sBuffer = new wchar_t[iBufferSize + 2];
        GetEnvironmentVariable(_T("APPDATA"), sBuffer, iBufferSize);
        directory.assign(u16to8(sBuffer));
        delete [] sBuffer;
    #else
        directory = std::string(std::getenv("HOME"));
    #endif

    directory.append("/mC2/");

    // Create folder if it does not exist
    boost::filesystem::path path(directory);
    if(!boost::filesystem::exists(path)) {
        boost::filesystem::create_directories(path);
    }

    return directory;
}

std::string musik::core::GetPath(const std::string &sFile){

    std::string sPath;
    int iStrLength;

#ifdef WIN32
    wchar_t szPath[2048];
    wchar_t *szFile = NULL;

    iStrLength = GetFullPathName(u8to16(sFile).c_str(), 2048, szPath, &szFile);
    if(iStrLength != 0 && iStrLength < 2048) {
        sPath.assign(u16to8(szPath).c_str());
        if(szFile!=0) {
            std::string sTheFile = u16to8(szFile);
            sPath.assign(sPath.substr(0,iStrLength-sTheFile.length()));
        }
    }
    else {
        sPath.assign(sFile);
    }
 #else	//TODO: check this POSIX GetPath works
    char* szDir;
    sPath.assign(getcwd((char*)szDir, (size_t) iStrLength));

 #endif //WIN32
    return sPath;
}

UINT64 musik::core::Checksum(char *data,unsigned int bytes){
    UINT64 sum(0);
    for(unsigned int i(0);i<bytes;++i){
        char theChar    = *(data+i);
        sum += (UINT64)theChar;
    }
    return sum;
}
