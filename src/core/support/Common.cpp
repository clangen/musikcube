//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <boost/format.hpp>

#ifdef WIN32
    /* nothing special for Win32 */
#elif __APPLE__
    #include <mach-o/dyld.h>
#else
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <limits.h>
#endif

std::string musik::core::GetPluginDirectory() {
    std::string path(GetApplicationDirectory());
    path.append("/plugins/");
    return path;
}

std::string musik::core::GetApplicationDirectory() {
    std::string result;

    #ifdef WIN32
        wchar_t widePath[2048];
        int length = GetModuleFileName(NULL, widePath, 2048);
        if (length != 0 && length < 2048) {
            result.assign(GetPath(u16to8(widePath).c_str()));
        }
    #elif __APPLE__
        char pathbuf[PATH_MAX + 1];
        uint32_t bufsize = sizeof(pathbuf);
        _NSGetExecutablePath(pathbuf, &bufsize);
        result.assign(pathbuf);
        size_t last = result.find_last_of("/");
        result = result.substr(0, last); /* remove filename component */
    #else
        std::string pathToProc = boost::str(boost::format("/proc/%d/exe") % (int) getpid());
        char pathbuf[PATH_MAX + 1];
        readlink(pathToProc.c_str(), pathbuf, PATH_MAX);
        result.assign(pathbuf);
        size_t last = result.find_last_of("/");
        result = result.substr(0, last); /* remove filename component */
    #endif

    return result;
}

std::string musik::core::GetHomeDirectory() {
    std::string directory;

#ifdef WIN32
    DWORD bufferSize = GetEnvironmentVariable(L"USERPROFILE", 0, 0);
    wchar_t *buffer = new wchar_t[bufferSize + 2];
    GetEnvironmentVariable(L"USERPROFILE", buffer, bufferSize);
    directory.assign(u16to8(buffer));
    delete[] buffer;
#else
    directory = std::string(std::getenv("HOME"));
#endif

    return directory;
}

static std::string GetDataDirectoryRoot() {
    std::string directory;

#ifdef WIN32
    DWORD bufferSize = GetEnvironmentVariable(L"APPDATA", 0, 0);
    wchar_t *buffer = new wchar_t[bufferSize + 2];
    GetEnvironmentVariable(L"APPDATA", buffer, bufferSize);
    directory.assign(u16to8(buffer));
    delete[] buffer;
#else
    directory = std::string(std::getenv("HOME"));
#endif

    return directory;
}

std::string musik::core::GetDataDirectory(bool create) {
    std::string directory =

#ifdef WIN32
    GetDataDirectoryRoot() + std::string("/musikcube/");
#else
    GetDataDirectoryRoot() + std::string("/.musikcube/");
#endif

    if (create) {
        boost::filesystem::path path(directory);
        if (!boost::filesystem::exists(path)) {
            boost::filesystem::create_directories(path);
        }
    }

    return directory;
}

void musik::core::MigrateOldDataDirectory() {
    std::string oldDirectory =

#ifdef WIN32
        GetDataDirectoryRoot() + std::string("/mC2/");
#else
        GetDataDirectoryRoot() + std::string("/.mC2/");
#endif

    std::string newDirectory = GetDataDirectory(false);

    boost::filesystem::path oldPath(oldDirectory);
    boost::filesystem::path newPath(newDirectory);

    if (boost::filesystem::exists(oldPath) &&
        !boost::filesystem::exists(newPath))
    {
        boost::filesystem::rename(oldPath, newPath);
    }
}

std::string musik::core::GetPath(const std::string &sFile) {
    std::string sPath;
    int length;

#ifdef WIN32
    wchar_t widePath[2048];
    wchar_t *szFile = NULL;

    length = GetFullPathName(u8to16(sFile).c_str(), 2048, widePath, &szFile);
    if(length != 0 && length < 2048) {
        sPath.assign(u16to8(widePath).c_str());
        if(szFile!=0) {
            std::string sTheFile = u16to8(szFile);
            sPath.assign(sPath.substr(0,length-sTheFile.length()));
        }
    }
    else {
        sPath.assign(sFile);
    }
 #else
    char* szDir;
    sPath.assign(getcwd((char*)szDir, (size_t) length));
 #endif

    return sPath;
}

musik_uint64 musik::core::Checksum(char *data,unsigned int bytes) {
    musik_uint64 sum = 0;
    for(unsigned int i = 0; i < bytes; ++i) {
        char ch = *(data + i);
        sum += (musik_uint64) ch;
    }
    return sum;
}

size_t musik::core::CopyString(const std::string& src, char* dst, size_t size) {
    size_t len = src.size() + 1; /* space for the null terminator */
    if (dst) {
        size_t copied = src.copy(dst, size - 1);
        dst[copied] = '\0';
        return copied + 1;
    }
    return len;
}

bool musik::core::FileToByteArray(const std::string& path, char** target, int& size, bool nullTerminate) {
#ifdef WIN32
    std::wstring u16fn = u8to16(path);
    FILE* file = _wfopen(u16fn.c_str(), L"rb");
#else
    FILE* file = fopen(path.c_str(), "rb");
#endif

    *target = nullptr;
    size = 0;

    if (!file) {
        return false;
    }

    bool success = false;

    if (fseek(file, 0L, SEEK_END) == 0) {
        long fileSize = ftell(file);
        if (fileSize == -1) {
            goto close_and_return;
        }

        if (fseek(file, 0L, SEEK_SET) != 0) {
            goto close_and_return;
        }

        *target = (char*)malloc(sizeof(char) * (fileSize + (nullTerminate ? 1 : 0)));
        size = fread(*target, sizeof(char), fileSize, file);

        if (size == fileSize) {
            if (nullTerminate) {
                (*target)[size] = 0;
            }

            success = true;
        }
    }

close_and_return:
    fclose(file);

    if (!success) {
        free(*target);
    }

    return success;
}