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

/** @file IPreferences.h @brief Defines the IPreferences interface for reading and writing persisted settings. */
#pragma once

#include <stddef.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A typed key/value store for plugin and application preferences,
     *  with values persisted between application sessions. */
    class IPreferences {
        public:
            /** @brief Releases the preferences; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Retrieves a value as a boolean.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual bool GetBool(const char* key, bool defaultValue = false) = 0;

            /** @brief Retrieves a value as an integer.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual int GetInt(const char* key, int defaultValue = 0) = 0;

            /** @brief Retrieves a value as a double.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual double GetDouble(const char* key, double defaultValue = 0.0f) = 0;

            /** @brief Retrieves a value as a string.
             *  @param key The key to look up.
             *  @param dst The destination buffer for the value.
             *  @param size The capacity of the destination buffer.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The number of bytes written, or the required size if the buffer was too small. */
            virtual int GetString(const char* key, char* dst, size_t size, const char* defaultValue = "") = 0;

            /** @brief Stores a boolean value.
             *  @param key The key to store.
             *  @param value The value to store. */
            virtual void SetBool(const char* key, bool value) = 0;

            /** @brief Stores an integer value.
             *  @param key The key to store.
             *  @param value The value to store. */
            virtual void SetInt(const char* key, int value) = 0;

            /** @brief Stores a double value.
             *  @param key The key to store.
             *  @param value The value to store. */
            virtual void SetDouble(const char* key, double value) = 0;

            /** @brief Stores a string value.
             *  @param key The key to store.
             *  @param value The value to store. */
            virtual void SetString(const char* key, const char* value) = 0;

            /** @brief Persists any pending changes to disk. */
            virtual void Save() = 0;
    };

    /** @brief Convenience helper that retrieves a preference string into a typed string.
     *  @tparam String The string type of the result.
     *  @param prefs The preferences object to read from.
     *  @param key The key to look up.
     *  @param defaultValue The value to return if the key is missing.
     *  @return The stored value, or the default. */
    template <typename String>
    String getPreferenceString(IPreferences* prefs, const char* key, const char* defaultValue) {
        if (prefs) {
            size_t count = prefs->GetString(key, nullptr, 0, defaultValue);
            if (count) {
                char* buffer = new char[count];
                prefs->GetString(key, buffer, count, defaultValue);
                String result = buffer;
                delete[] buffer;
                return result;
            }
        }
        return "";
    }

} } }

