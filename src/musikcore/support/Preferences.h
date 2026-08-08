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

/** @file Preferences.h
 *  @brief JSON-backed persistent settings with component-based storage.
 *  @details Each component has its own preferences file loaded from the data
 *      directory. Supports read-only, read-write, transient and auto-save modes,
 *      and exposes both the SDK IPreferences interface and a friendlier
 *      std::string-based interface. */

#include <memory>
#include <vector>
#include <mutex>

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>
#include <musikcore/sdk/IPreferences.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {
    /** @brief A JSON-backed store of key/value preferences.
     *  @details Loads a component-specific JSON file on construction. Reads and
     *      writes are serialized through an internal mutex. In ModeAutoSave,
     *      writes are persisted automatically; in other modes, Save() writes. */
    class Preferences : public musik::core::sdk::IPreferences {
        public:
            /** @brief Persistence behavior of the preferences store. */
            enum Mode {
                ModeTransient, /**< In-memory only; never persisted. */
                ModeReadOnly,  /**< Loaded from disk but writes are ignored. */
                ModeReadWrite, /**< Reads and writes; persisted on Save(). */
                ModeAutoSave   /**< Reads and writes; persisted automatically. */
            };

            /** @brief Loads and caches all plugin preference stores. */
            static void LoadPluginPreferences();
            /** @brief Saves all plugin preference stores. */
            static void SavePluginPreferences();

            /** @return An unmanaged (raw) preferences store for a component.
             *  @param name The component name. */
            static musik::core::sdk::IPreferences* Unmanaged(const std::string& name);

            /** @return A shared preferences store for a plugin.
             *  @param pluginName The plugin name. */
            static std::shared_ptr<Preferences>
                ForPlugin(const std::string& pluginName);

            /** @return A shared preferences store for a component.
             *  @param c The component name.
             *  @param mode The persistence mode. */
            static std::shared_ptr<Preferences>
                ForComponent(const std::string& c, Mode mode = ModeAutoSave);

            ~Preferences();

            /* IPreferences (for plugin use) */
            /** @brief Frees the preferences (deletes this instance). */
            virtual void Release() override;

            /** @brief Reads a boolean value.
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            bool GetBool(const char* key, bool defaultValue = false) override;
            /** @brief Reads an integer value.
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            int GetInt(const char* key, int defaultValue = 0) override;
            /** @brief Reads a double value.
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            double GetDouble(const char* key, double defaultValue = 0.0f) override;
            /** @brief Reads a string value.
             *  @param key The preference key.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @param defaultValue Value if the key is missing.
             *  @return The number of bytes copied. */
            int GetString(const char* key, char* dst, size_t size, const char* defaultValue = "") override;

            /** @brief Writes a boolean value.
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetBool(const char* key, bool value) override;
            /** @brief Writes an integer value.
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetInt(const char* key, int value) override;
            /** @brief Writes a double value.
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetDouble(const char* key, double value) override;
            /** @brief Writes a string value.
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetString(const char* key, const char* value) override;

            /** @brief Persists the preferences to disk (per the mode). */
            void Save() override;

            /* easier interface for internal use */
            /** @brief Reads a boolean value (std::string key).
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            bool GetBool(const std::string& key, bool defaultValue = false);
            /** @brief Reads an integer value (std::string key).
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            int GetInt(const std::string& key, int defaultValue = 0);
            /** @brief Reads a double value (std::string key).
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            double GetDouble(const std::string& key, double defaultValue = 0.0f);
            /** @brief Reads a string value (std::string key).
             *  @param key The preference key.
             *  @param defaultValue Value if the key is missing.
             *  @return The value. */
            std::string GetString(const std::string& key, const std::string& defaultValue = "");

            /** @brief Writes a boolean value (std::string key).
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetBool(const std::string& key, bool value);
            /** @brief Writes an integer value (std::string key).
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetInt(const std::string& key, int value);
            /** @brief Writes a double value (std::string key).
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetDouble(const std::string& key, double value);
            /** @brief Writes a string value (std::string key).
             *  @param key The preference key.
             *  @param value The value to store. */
            void SetString(const std::string& key, const char* value);

            /** @brief Collects all stored keys.
             *  @param target Output vector receiving the keys. */
            void GetKeys(std::vector<std::string>& target);
            /** @return true if the given key is present.
             *  @param key The preference key. */
            bool HasKey(const std::string& key);
            /** @brief Removes a key from the store.
             *  @param key The preference key. */
            void Remove(const std::string& key);

        private:
            /** @brief Constructs a store for a component.
             *  @param component The component name.
             *  @param mode The persistence mode. */
            Preferences(const std::string& component, Mode mode);
            /** @brief Loads the component's JSON file from disk. */
            void Load();

            std::mutex mutex; /**< Serializes access. */
            nlohmann::json json; /**< Underlying key/value data. */
            std::string component; /**< Component (file) name. */
            Mode mode; /**< Persistence mode. */
    };

} }

