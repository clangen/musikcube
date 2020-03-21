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

#include <memory>
#include <vector>
#include <mutex>

#include <core/config.h>
#include <core/db/Connection.h>
#include <core/sdk/IPreferences.h>
#include <json.hpp>

namespace musik { namespace core {
    class Preferences : public musik::core::sdk::IPreferences {
        public:
            enum Mode {
                ModeTransient,
                ModeReadOnly,
                ModeReadWrite,
                ModeAutoSave
            };

            static void LoadPluginPreferences();
            static void SavePluginPreferences();

            static musik::core::sdk::IPreferences* Unmanaged(const std::string& name);

            static std::shared_ptr<Preferences>
                ForPlugin(const std::string& pluginName);

            static std::shared_ptr<Preferences>
                ForComponent(const std::string& c, Mode mode = ModeAutoSave);

            ~Preferences();

            /* IPreferences (for plugin use) */
            virtual void Release() override;

            virtual bool GetBool(const char* key, bool defaultValue = false) override;
            virtual int GetInt(const char* key, int defaultValue = 0) override;
            virtual double GetDouble(const char* key, double defaultValue = 0.0f) override;
            virtual int GetString(const char* key, char* dst, size_t size, const char* defaultValue = "") override;

            virtual void SetBool(const char* key, bool value) override;
            virtual void SetInt(const char* key, int value) override;
            virtual void SetDouble(const char* key, double value) override;
            virtual void SetString(const char* key, const char* value) override;

            virtual void Save() override;

            /* easier interface for internal use */
            virtual bool GetBool(const std::string& key, bool defaultValue = false);
            virtual int GetInt(const std::string& key, int defaultValue = 0);
            virtual double GetDouble(const std::string& key, double defaultValue = 0.0f);
            virtual std::string GetString(const std::string& key, const std::string& defaultValue = "");

            virtual void SetBool(const std::string& key, bool value);
            virtual void SetInt(const std::string& key, int value);
            virtual void SetDouble(const std::string& key, double value);
            virtual void SetString(const std::string& key, const char* value);

            void GetKeys(std::vector<std::string>& target);
            bool HasKey(const std::string& key);
            void Remove(const std::string& key);

        private:
            Preferences(const std::string& component, Mode mode);
            void Load();

            std::mutex mutex;
            nlohmann::json json;
            std::string component;
            Mode mode;
    };

} }

