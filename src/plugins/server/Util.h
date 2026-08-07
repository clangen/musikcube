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

/// @file Util.h
/// @brief Small helpers shared by the streaming server modules.
/// @details Provides template functions for looking up values in maps and
/// reading metadata/preference strings through the thread-local buffer, plus
/// URL encode/decode helpers and a UTF-8 to UTF-16 conversion for Windows.

#include <string>
#include <algorithm>
#include <unordered_map>

#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/IValue.h>

#ifdef WIN32
#include <Windows.h>
#endif

#ifdef __APPLE__
/** @brief Thread-local scratch buffer shared by the string helpers. */
extern __thread char threadLocalBuffer[4096];
#else
/** @brief Thread-local scratch buffer shared by the string helpers. */
extern thread_local char threadLocalBuffer[4096];
#endif

/** @brief Finds an iterator for the first map entry whose value matches.
 *  @tparam K Map key type.
 *  @tparam V Map value type.
 *  @param map The map to search.
 *  @param value The value to look up.
 *  @return Const iterator to the matching entry, or map.end(). */
template <typename K, typename V>
typename std::unordered_map<K, V>::const_iterator
static FindKeyByValue(const std::unordered_map<K, V>& map, const V& value) {
    return std::find_if(map.begin(), map.end(), [&value](const std::pair<K, V> &p) {
        return p.second == value;
    });
}

/** @brief Reads a string preference with a fallback default.
 *  @param prefs The preferences service.
 *  @param key The preference key.
 *  @param defaultValue Value returned when the key is absent.
 *  @return The preference value. */
static std::string GetPreferenceString(
    musik::core::sdk::IPreferences* prefs,
    const std::string& key,
    const std::string& defaultValue)
{
    prefs->GetString(key.c_str(), threadLocalBuffer, sizeof(threadLocalBuffer), defaultValue.c_str());
    return std::string(threadLocalBuffer);
}

/** @brief Reads a string from a metadata object.
 *  @tparam MetadataT Metadata type exposing GetString.
 *  @param metadata The metadata object.
 *  @param key The metadata key.
 *  @param defaultValue Value returned when the key is absent.
 *  @return The metadata value. */
template <typename MetadataT>
static std::string GetMetadataString(
    MetadataT* metadata,
    const std::string& key,
    const std::string& defaultValue = "missing metadata!")
{
    if (!metadata) { return defaultValue; }
    metadata->GetString(key.c_str(), threadLocalBuffer, sizeof(threadLocalBuffer));
    return std::string(threadLocalBuffer);
}

/** @brief Reads an int32 from a metadata object.
 *  @tparam MetadataT Metadata type exposing GetInt32.
 *  @param metadata The metadata object.
 *  @param key The metadata key.
 *  @param defaultValue Value returned when the key is absent.
 *  @return The metadata value. */
template <typename MetadataT>
static int GetMetadataInt32(MetadataT* metadata, const std::string& key, int defaultValue = 0) {
    return !metadata ? defaultValue : metadata->GetInt32(key.c_str(), defaultValue);
}

/** @brief Reads an int64 from a metadata object.
 *  @tparam MetadataT Metadata type exposing GetInt64.
 *  @param metadata The metadata object.
 *  @param key The metadata key.
 *  @param defaultValue Value returned when the key is absent.
 *  @return The metadata value. */
template <typename MetadataT>
static int64_t GetMetadataInt64(MetadataT* metadata, const std::string& key, int64_t defaultValue = 0LL) {
    return !metadata ? defaultValue : metadata->GetInt64(key.c_str(), defaultValue);
}

/** @brief Reads the value of an IValue as a string.
 *  @param value The IValue to read.
 *  @param defaultValue Value returned when value is null.
 *  @return The value string. */
static std::string GetValueString(
    musik::core::sdk::IValue* value,
    const std::string& defaultValue = "missing metadata!")
{
    if (!value) { return defaultValue; }
    value->GetValue(threadLocalBuffer, sizeof(threadLocalBuffer));
    return std::string(threadLocalBuffer);
}

/** @brief Creates an IValue instance.
 *  @param value The string value.
 *  @param id The numeric id.
 *  @param type The value type name.
 *  @return A new IValue the caller must release. */
extern musik::core::sdk::IValue* CreateValue(
    const std::string& value, int64_t id, const std::string& type);

/** @brief URL-encodes a string.
 *  @param s The string to encode.
 *  @return The percent-encoded string. */
extern std::string urlEncode(const std::string& s);

/** @brief URL-decodes a string.
 *  @param str The percent-encoded string.
 *  @return The decoded string. */
extern std::string urlDecode(const std::string& str);

#ifdef WIN32
/** @brief Converts a UTF-8 string to a UTF-16 wide string (Windows only).
 *  @param utf8 The UTF-8 input string.
 *  @return The UTF-16 string. */
static inline std::wstring utf8to16(const char* utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    if (size <= 0) return L"";
    wchar_t* buffer = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, size);
    std::wstring utf16fn(buffer);
    delete[] buffer;
    return utf16fn;
}
#endif
