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

#include <core/sdk/ISchema.h>
#include <string>
#include <set>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#include <Windows.h>
#include <io.h>
#define R_OK 0
#else
#define DLLEXPORT
#include <dirent.h>
#include <unistd.h>
#endif

static const char* PLUGIN_NAME = "GME IDecoder";

static const char* KEY_ALWAYS_LOOP_FOREVER = "always_loop_forever";
static const bool DEFAULT_ALWAYS_LOOP_FOREVER = false;
static const char* KEY_IGNORE_EMBEDDED_TRACK_LENGTH = "ignore_embedded_track_length";
static const bool DEFAULT_IGNORE_EMBEDDED_TRACK_LENGTH = false;
static const char* KEY_DEFAULT_TRACK_LENGTH = "default_track_length_secs";
static const double DEFAULT_TRACK_LENGTH = 60.0 * 3.0;
static const char* KEY_TRACK_FADE_OUT_LENGTH = "track_fade_out_length_secs";
static const double DEFAULT_FADE_OUT_LENGTH = 3.0;
static const char* KEY_ENABLE_M3U = "enable_m3u_support";
static const bool DEFAULT_ENABLE_M3U = false;
static const char* KEY_MINIMUM_TRACK_LENGTH = "minimum_track_length_secs";
static const double DEFAULT_MINIMUM_TRACK_LENGTH = 0.0;
static const char* KEY_EXCLUDE_SOUND_EFFECTS = "exclude_tracks_shorter_than_minimum_length";
static const bool DEFAULT_EXCLUDE_SOUND_EFFECTS = false;

static const std::set<std::string> FORMATS = {
    ".vgm", ".gym", ".spc", ".sap", ".nsfe",
    ".nsf", ".ay", ".gbs", ".hes", ".kss"
};

#ifdef WIN32
static inline std::string u16to8(const wchar_t* utf16) {
    if (!utf16) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, 0, 0, 0, 0);
    if (size <= 0) return "";
    char* buffer = new char[size];
    WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buffer, size, 0, 0);
    std::string utf8str(buffer);
    delete[] buffer;
    return utf8str;
}

static inline std::wstring u8to16(const char* utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    if (size <= 0) return L"";
    wchar_t* buffer = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, size);
    std::wstring utf16fn(buffer);
    delete[] buffer;
    return utf16fn;
}
#endif

static inline musik::core::sdk::ISchema* CreateSchema() {
    auto schema = new musik::core::sdk::TSchema<>();
    schema->AddBool(KEY_ALWAYS_LOOP_FOREVER, DEFAULT_ALWAYS_LOOP_FOREVER);
    schema->AddBool(KEY_IGNORE_EMBEDDED_TRACK_LENGTH, DEFAULT_IGNORE_EMBEDDED_TRACK_LENGTH);
    schema->AddDouble(KEY_DEFAULT_TRACK_LENGTH, DEFAULT_TRACK_LENGTH);
    schema->AddDouble(KEY_TRACK_FADE_OUT_LENGTH, DEFAULT_FADE_OUT_LENGTH);
    schema->AddDouble(KEY_MINIMUM_TRACK_LENGTH, DEFAULT_MINIMUM_TRACK_LENGTH);
    schema->AddBool(KEY_EXCLUDE_SOUND_EFFECTS, DEFAULT_EXCLUDE_SOUND_EFFECTS);
    schema->AddBool(KEY_ENABLE_M3U, DEFAULT_ENABLE_M3U);
    return schema;
}

static inline bool canHandle(std::string fn) {
    std::transform(fn.begin(), fn.end(), fn.begin(), ::tolower);
    for (auto& ext : FORMATS) {
        if (fn.size() >= ext.size() &&
            fn.rfind(ext) == fn.size() - ext.size())
        {
            return true;
        }
    }
    return false;
}

static inline bool parseExternalId(const std::string& externalId, std::string& fn, int& track) {
    if (externalId.find("gme://") == 0) {
        std::string trimmed = externalId.substr(6);
        auto slash = trimmed.find("/");
        if (slash != std::string::npos) {
            try {
                track = std::stoi(trimmed.substr(0, slash));
                fn = trimmed.substr(slash + 1);
                return true;
            }
            catch (...) {
                return false;
            }
        }
    }
    return false;
}

static inline std::string createExternalId(const std::string& fn, int track) {
    return "gme://" + std::to_string(track) + "/" + fn;
}

template<typename... Args>
static std::string strfmt(const std::string& format, Args ... args) {
    /* https://stackoverflow.com/a/26221725 */
    size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; /* extra space for '\0' */
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); /* omit the '\0' */
}

static std::string getM3uFor(const std::string& fn) {
    size_t lastDot = fn.find_last_of(".");
    if (lastDot != std::string::npos) {
        std::string m3u = fn.substr(0, lastDot) + ".m3u";
#ifdef WIN32
        auto m3u16 = u8to16(m3u.c_str());
        if (_waccess(m3u16.c_str(), R_OK) != -1) {
            return m3u;
        }
#else
        if (access(m3u.c_str(), R_OK) != -1) {
            return m3u;
        }
#endif
    }
    return "";
}

static inline bool fileExists(const std::string& fn) {
#ifdef WIN32
    auto fn16 = u8to16(fn.c_str());
    return _waccess(fn16.c_str(), R_OK) != -1;
#else
    return access(fn.c_str(), R_OK) != -1;
#endif
}

static inline bool externalIdExists(const std::string& externalId) {
    std::string fn;
    int trackNum;
    if (!parseExternalId(externalId, fn, trackNum)) {
        return false;
    }
    return fileExists(fn);
}

static int getLastModifiedTime(const std::string& fn) {
#ifdef WIN32
    /* todo */
    struct _stat result = { 0 };
    std::wstring fn16 = u8to16(fn.c_str());
    if (_wstat(fn16.c_str(), &result) == 0) {
        return (int) result.st_mtime;
    }
#else
    struct stat result = { 0 };
    if (stat(fn.c_str(), &result) == 0) {
        return result.st_mtime;
    }
#endif
    return -1;
}

static inline std::string canonicalizePath(const std::string& path) {
#ifdef WIN32
    std::wstring path16 = u8to16(path.c_str());
    std::string result8;
    DWORD size = GetFullPathName(path16.c_str(), 0, 0, nullptr);
    if (size) {
        wchar_t* dest = new wchar_t[size];
        if (GetFullPathName(path16.c_str(), size, dest, nullptr)) {
            result8 = u16to8(dest);
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
