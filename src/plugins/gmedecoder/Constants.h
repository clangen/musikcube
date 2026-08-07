//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/// @file Constants.h
/// @brief Shared constants, settings schema and helpers for the GME decoder.
/// @details Declares the plugin name, the supported video-game music file
/// extensions handled by the Game Music Emu library, the configurable settings
/// (looping, track length, fade-out, M3U support) and small helpers used by
/// the decoder and indexer source.

#include <musikcore/sdk/ISchema.h>
#include <musikcore/sdk/String.h>
#include <string>
#include <set>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#include <Windows.h>
#include <io.h>
#define R_OK 0
#else
#include <unistd.h>
#define DLLEXPORT
#endif

/** @brief Display name of the GME decoder plugin. */
static const char* PLUGIN_NAME = "GME IDecoder";

/** @brief Prefix used for externally generated track ids. */
static const std::string EXTERNAL_ID_PREFIX = "gme";

/** @brief Setting key controlling whether tracks loop forever. */
static const char* KEY_ALWAYS_LOOP_FOREVER = "always_loop_forever";
/** @brief Default for always_loop_forever. */
static const bool DEFAULT_ALWAYS_LOOP_FOREVER = false;
/** @brief Setting key controlling whether the embedded track length is ignored. */
static const char* KEY_IGNORE_EMBEDDED_TRACK_LENGTH = "ignore_embedded_track_length";
/** @brief Default for ignore_embedded_track_length. */
static const bool DEFAULT_IGNORE_EMBEDDED_TRACK_LENGTH = false;
/** @brief Setting key for the default track length in seconds. */
static const char* KEY_DEFAULT_TRACK_LENGTH = "default_track_length_secs";
/** @brief Default track length (3 minutes). */
static const double DEFAULT_TRACK_LENGTH = 60.0 * 3.0;
/** @brief Setting key for the fade-out length in seconds. */
static const char* KEY_TRACK_FADE_OUT_LENGTH = "track_fade_out_length_secs";
/** @brief Default fade-out length (3 seconds). */
static const double DEFAULT_FADE_OUT_LENGTH = 3.0;
/** @brief Setting key enabling M3U playlist support. */
static const char* KEY_ENABLE_M3U = "enable_m3u_support";
/** @brief Default for enable_m3u_support. */
static const bool DEFAULT_ENABLE_M3U = false;
/** @brief Setting key for the minimum track length in seconds. */
static const char* KEY_MINIMUM_TRACK_LENGTH = "minimum_track_length_secs";
/** @brief Default minimum track length. */
static const double DEFAULT_MINIMUM_TRACK_LENGTH = 0.0;
/** @brief Setting key excluding tracks shorter than the minimum length. */
static const char* KEY_EXCLUDE_SOUND_EFFECTS = "exclude_tracks_shorter_than_minimum_length";
/** @brief Default for exclude_tracks_shorter_than_minimum_length. */
static const bool DEFAULT_EXCLUDE_SOUND_EFFECTS = false;

/** @brief File extensions supported by the GME decoder plugin. */
static const std::set<std::string> FORMATS = {
    ".vgm", ".gym", ".spc", ".sap", ".nsfe",
    ".nsf", ".ay", ".gbs", ".hes", ".kss"
};

/** @brief Creates the settings schema registered with the settings system.
 *  @return A newly allocated ISchema with all GME plugin settings. */
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

/** @brief Returns whether a filename has a supported GME extension.
 *  @param fn The filename to check (compared case-insensitively).
 *  @return True if the extension is in FORMATS. */
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

/** @brief Locates the companion M3U playlist for a chiptune file.
 *  @param fn The chiptune file path.
 *  @return The path of the M3U file if it exists, otherwise an empty string. */
static std::string getM3uFor(const std::string& fn) {
    size_t lastDot = fn.find_last_of(".");
    if (lastDot != std::string::npos) {
        std::string m3u = fn.substr(0, lastDot) + ".m3u";
#ifdef WIN32
        auto m3u16 = musik::core::sdk::str::u8to16(m3u.c_str());
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
