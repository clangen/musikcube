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

#include <initializer_list>
#include <string_view>
#include <memory>
#include <vector>
#include <mutex>

#include <musikcore/config.h>
#include <musikcore/settings.h>
#include <musikcore/db/Connection.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace musik { namespace core {
    class Settings {
        public:
            enum Mode {
                ModeReadOnly,
                ModeReadWrite,
                ModeAutoSave
            };

            static void SavePluginSettings();

            static std::shared_ptr<Settings>
                ForPlugin(const std::string& pluginName);

            static std::shared_ptr<Settings>
                ForComponent(const std::string& c, Mode mode = ModeAutoSave);

            ~Settings();

            void Save();

            bool GetBool(const std::string& key, bool defaultValue = false);
            /* read the first saved value of the requested type. a missing key
            stays missing so it can continue to inherit its default. */
            template<typename T> T Lookup(std::initializer_list<std::string_view> keys, T fallback) {
                std::lock_guard lock(this->mutex);
                for (auto key : keys) {
                    const auto value = this->json.find(key);
                    if (value != this->json.end()) {
                        try {
                            return value->get<T>();
                        }
                        catch (const nlohmann::json::type_error&) {
                            /* an invalid setting leaves the next choice eligible. */
                        }
                    }
                }
                return fallback;
            }

            int GetInt(const std::string& key, int defaultValue = 0);
            double GetDouble(const std::string& key, double defaultValue = 0.0f);
            std::string GetString(const std::string& key, const std::string& defaultValue = "");
            /* Text values borrow the caller's output storage. */
            mc_setting_value ReadValue(const std::string& key, mc_setting_value fallback, std::string& text);
            static nlohmann::json EncodeValue(const mc_setting_value& value);

            void SetBool(const std::string& key, bool value);
            void SetInt(const std::string& key, int value);
            void SetDouble(const std::string& key, double value);
            void SetString(const std::string& key, const char* value);
            bool SetValues(const nlohmann::json& values, bool persist = false);
            bool Flush();

            void GetKeys(std::vector<std::string>& target);
            bool HasKey(const std::string& key);
            void Remove(const std::string& key);

        private:
            Settings(const std::string& component, Mode mode);
            void Load();

            std::mutex mutex;
            nlohmann::json json;
            std::string component;
            Mode mode;
    };

} }

