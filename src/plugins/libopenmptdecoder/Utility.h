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

/// @file Utility.h
/// @brief Shared constants, helpers and settings schema for the libopenmpt plugin.
/// @details Declares the plugin name, the external id prefix, the configurable
/// default album/artist name templates and the helper functions shared by the
/// decoder, indexer source and plugin entry point.

#include <string>

#include <musikcore/sdk/ISchema.h>

#pragma warning(push, 0)
#include <libopenmpt/libopenmpt.h>
#pragma warning(pop)

/** @brief Display name of the libopenmpt decoder plugin. */
static const std::string PLUGIN_NAME = "libopenmpt";
/** @brief Prefix used for externally generated track ids. */
static const std::string EXTERNAL_ID_PREFIX = "libopenmpt";

/** @brief Setting key for the default album name template. */
static const char* KEY_DEFAULT_ALBUM_NAME = "default_album_name";
/** @brief Default album name used when a module has no album tag. */
static const char* DEFAULT_ALBUM_NAME = "[unknown %s album]";
/** @brief Setting key for the default artist name template. */
static const char* KEY_DEFAULT_ARTIST_NAME = "default_artist_name";
/** @brief Default artist name used when a module has no artist tag. */
static const char* DEFAULT_ARTIST_NAME = "[unknown %s artist]";

/** @brief Returns whether the given stream type is handled by this plugin.
 *  @param type The stream type to check.
 *  @return True for supported tracker stream types. */
extern bool isFileTypeSupported(const char* type);
/** @brief Returns whether a filename has a supported tracker extension.
 *  @param filename The filename to check.
 *  @return True if the extension is supported. */
extern bool isFileSupported(const std::string& filename);
/** @brief Reads a file fully into an allocated byte array.
 *  @param path The file path to read.
 *  @param target Receives a newly allocated buffer (caller frees).
 *  @param size Receives the number of bytes read.
 *  @return True if the file was read successfully. */
extern bool fileToByteArray(const std::string& path, char** target, int& size);
/** @brief Reads a metadata value from an open module.
 *  @param module The open module.
 *  @param key The metadata key to look up.
 *  @param defaultValue Value returned when the key is absent.
 *  @return The metadata value or the default. */
extern std::string readMetadataValue(openmpt_module* module, const char* key, const char* defaultValue = "");
/** @brief Creates the settings schema registered with the settings system.
 *  @return A newly allocated ISchema with all libopenmpt plugin settings. */
extern musik::core::sdk::ISchema* createSchema();