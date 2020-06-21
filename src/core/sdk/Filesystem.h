//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <string>
#include <set>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <functional>
#include "String.h"

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#define R_OK 0
#else
#define DLLEXPORT
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace musik { namespace core { namespace sdk { namespace fs {

    template <typename String=std::string>
    static inline bool fileExists(const String& fn) {
    #ifdef WIN32
        auto fn16 = musik::core::sdk::str::u8to16(fn.c_str());
        return _waccess(fn16.c_str(), R_OK) != -1;
    #else
        return access(fn.c_str(), R_OK) != -1;
    #endif
    }

    template <typename String=std::string>
    static int getLastModifiedTime(const String& fn) {
    #ifdef WIN32
        /* todo */
        struct _stat result = { 0 };
        std::wstring fn16 = musik::core::sdk::str::u8to16(fn.c_str());
        if (_wstat(fn16.c_str(), &result) == 0) {
            return (int)result.st_mtime;
        }
    #else
        struct stat result = { 0 };
        if (stat(fn.c_str(), &result) == 0) {
            return result.st_mtime;
        }
    #endif
        return -1;
    }

    template <typename String=std::string>
    static inline std::string canonicalizePath(const String& path) {
    #ifdef WIN32
        std::wstring path16 = musik::core::sdk::str::u8to16(path.c_str());
        std::string result8;
        DWORD size = GetFullPathName(path16.c_str(), 0, 0, nullptr);
        if (size) {
            wchar_t* dest = new wchar_t[size];
            if (GetFullPathName(path16.c_str(), size, dest, nullptr)) {
                result8 = musik::core::sdk::str::u16to8(dest);
            }
            delete[] dest;
        }
        return result8;
    #else
        char* realname = realpath(path.c_str(), nullptr);
        if (!realname) {
            return "";
        }
        std::string result = realname;
        free(realname);
        return result;
    #endif
    }

    template <typename String=std::string>
    static inline std::string getFileExtension(const String& filename) {
        std::string::size_type lastDot = filename.find_last_of(".");
        if (lastDot != std::string::npos) {
            return filename.substr(lastDot + 1).c_str();
        }
        return "";
    }

    template <typename String=std::string>
    static inline std::string getDirectory(const String& filename) {
        std::string canonicalized = canonicalizePath(filename);
#ifdef WIN32
        size_t pos = canonicalized.find_last_of("\\");
#else
        size_t pos = canonicalized.find_last_of("/");
#endif
        if (pos != std::string::npos) {
            std::string result = canonicalized.substr(0, pos + 1);
            return result;
        }
        return "";
    }

    template <typename String=std::string>
    void scanDirectory(
        const std::string& path,
        std::function<void(const std::string&)> callback,
        std::function<bool()> interrupt = std::function<bool()>())
    {
#ifdef WIN32
        auto path16 = str::u8to16(path.c_str()) + L"*";
        WIN32_FIND_DATA findData;
        HANDLE handle = FindFirstFile(path16.c_str(), &findData);

        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }

        while (FindNextFile(handle, &findData)) {
            if (!findData.cFileName) {
                continue;
            }
            bool hasTrailingSlash = path[path.size() - 1] == '\\';
            std::string relPath8 = str::u16to8(findData.cFileName);
            std::string fullPath8 = path + (hasTrailingSlash ? "" : "\\") + relPath8;
            if (interrupt && interrupt()) {
                FindClose(handle);
                return;
            }
            else if (findData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
                if (relPath8 != "." && relPath8 != "..") {
                    scanDirectory(fullPath8 + "\\", callback, interrupt);
                }
            }
            else {
                callback(fullPath8);
            }
        }

        FindClose(handle);
#else
        DIR *dir = nullptr;
        struct dirent *entry = nullptr;

        if (!(dir = opendir(path.c_str()))) {
            return;
        }

        while ((entry = readdir(dir)) != nullptr) {
            if (interrupt && interrupt()) {
                closedir(dir);
                return;
            }

            bool hasTrailingSlash = path[path.size() - 1] == '/';
            std::string fn = path + (hasTrailingSlash ? "" : "/") + entry->d_name;

            struct stat info = {0};
            if (stat(fn.c_str(), &info) < 0) {
                return;
            }
            if (S_ISDIR(info.st_mode)) {
                std::string name = entry->d_name;
                if (name == "." || name == "..") {
                    continue;
                }
                scanDirectory(path + "/" + name, callback, interrupt);
            }
            else {
                callback(fn);
            }
        }

        closedir(dir);
#endif
    }

} } } }
