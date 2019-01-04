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

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

static const std::string PLUGIN_NAME = "GME IDecoder";

static const std::set<std::string> FORMATS = {
    ".vgm", ".gym", ".spc", ".sap", ".nsfe",
    ".nsf", ".ay", ".gbs", ".hes", ".kss"
};

static inline bool canHandle(std::string fn) {
    std::transform(fn.begin(), fn.end(), fn.begin(), ::tolower);
    for (auto& ext : FORMATS) {
        if (fn.rfind(ext) == fn.size() - ext.size()) {
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