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

#include "pch.hpp"

#include <musikcore/support/Settings.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/SettingKeys.h>
#include <musikcore/ffi/consumer/settings-shape.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/debug.h>
#include <unordered_map>

using nlohmann::json;
using namespace musik;
using namespace musik::core;

static const char* TAG = "Settings";
static std::unordered_map<std::string, std::weak_ptr<Settings>> componentCache;
static std::unordered_map<std::string, std::shared_ptr<Settings>> pluginCache;
static std::mutex cacheMutex;

#define CACHE_KEY(name, mode) u8fmt("%s-%d", name.c_str(), mode)

#define FILENAME(x) musik::core::GetDataDirectory() + "/" + x + ".json"

static FILE* openFile(const std::string& fn, const std::string& mode) {
#ifdef WIN32
    std::wstring u16fn = u8to16(fn);
    std::wstring u16mode = u8to16(mode);
    return _wfopen(u16fn.c_str(), u16mode.c_str());
#else
    return fopen(fn.c_str(), mode.c_str());
#endif
}

static std::string fileToString(const std::string& fn) {
    std::string result;

    if (fn.size()) {
        FILE* f = openFile(fn, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            rewind(f);

            if (len > 0) {
                char* bytes = new char[len];
                fread(static_cast<void*>(bytes), len, 1, f);
                result.assign(bytes, len);
                delete[] bytes;
            }

            fclose(f);
        }
    }

    return result;
}

static bool stringToFile(const std::string& fn, const std::string& str) {
    FILE* f = openFile(fn, "wb");

    if (!f) {
        return false;
    }

    const auto written = fwrite(str.data(), 1, str.size(), f);
    const auto closed = fclose(f);
    return written == str.size() && closed == 0;
}

static std::string pluginFilename(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    name = "plugin_" + name; /* filename = nowhitespace(tolower(name)).json */
    return name;
}

void Settings::SavePluginSettings() {
    std::vector<std::shared_ptr<Settings>> stores;
    {
        std::lock_guard lock(cacheMutex);
        stores.reserve(pluginCache.size());
        for (const auto& [name, store] : pluginCache) {
            stores.push_back(store);
        }
    }

    /* Providers keep borrowed setting pointers until their modules unload. */
    for (const auto& store : stores) {
        store->Save();
    }
}

std::shared_ptr<Settings> Settings::ForPlugin(const std::string& pluginName) {
    std::string name = pluginFilename(pluginName);
    std::lock_guard lock(cacheMutex);

    if (pluginCache.find(name) == pluginCache.end()) {
        pluginCache[name] = std::shared_ptr<Settings>(
            new Settings(name, Settings::ModeAutoSave));
    }

    return pluginCache[name];
}

std::shared_ptr<Settings> Settings::ForComponent(
    const std::string& c, Settings::Mode mode)
{
    std::unique_lock<std::mutex> lock(cacheMutex);

    std::string key = CACHE_KEY(c, mode);

    auto it = componentCache.find(key);
    if (it != componentCache.end()) {
        auto weak = it->second;
        try {
            auto shared = weak.lock();
            if (shared) {
                return shared;
            }
        }
        catch (...) {
            /* unable to lock, let's create a new one... */
        }
    }

    std::shared_ptr<Settings> prefs(new Settings(c, mode));
    componentCache[key] = prefs;
    return prefs;
}

Settings::Settings(const std::string& component, Mode mode) {
    this->mode = mode;
    this->component = component;
    this->Load();
}

Settings::~Settings() {
    if (this->mode == ModeAutoSave) {
        this->Save();
    }
}

#define RETURN_VALUE(defaultValue) \
{ \
    try { \
        auto it = json.find(key); \
        if (it == json.end()) { \
            json[key] = defaultValue; \
            return defaultValue; \
        } \
        return it.value(); \
    } \
    catch (...) { \
        return defaultValue; \
    } \
}

bool Settings::GetBool(const std::string& key, bool defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

int Settings::GetInt(const std::string& key, int defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

double Settings::GetDouble(const std::string& key, double defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

std::string Settings::GetString(const std::string& key, const std::string& defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    try {
        auto it = json.find(key);
        if (it == json.end()) {
            json[key] = defaultValue;
            return defaultValue;
        }
        std::string value = it.value();
        if (!value.size() && defaultValue.size()) {
            json[key] = defaultValue;
            return defaultValue;
        }
        return value;
    }
    catch (...) {
        return defaultValue;
    }
}

void Settings::SetBool(const std::string& key, bool value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Settings::SetInt(const std::string& key, int value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Settings::SetDouble(const std::string& key, double value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Settings::SetString(const std::string& key, const char* value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

bool Settings::SetValues(const nlohmann::json& values, bool persist) {
    std::lock_guard lock(this->mutex);
    auto updated = this->json.is_object() ? this->json : json::object();
    updated.update(values);
    if (persist && (this->mode == ModeReadOnly ||
        !stringToFile(FILENAME(this->component), updated.dump(2))))
    {
        return false;
    }
    this->json.swap(updated);
    return true;
}

void Settings::GetKeys(std::vector<std::string>& target) {
    std::lock_guard lock(this->mutex);
    auto it = json.begin();
    for (; it != json.end(); it++) {
        target.push_back(it.key());
    }
}

bool Settings::HasKey(const std::string& key) {
    return json.find(key) != json.end();
}

void Settings::Remove(const std::string& key) {
    auto it = json.find(key);
    if (it != json.end()) {
        json.erase(it);
    }
}

void Settings::Load() {
    std::string str = fileToString(FILENAME(this->component));
    if (str.size()) {
        try {
            this->json = json::parse(str);
        }
        catch (...) {
            debug::error(TAG, u8fmt("error loading %s.json", component.c_str()).c_str());
            this->json = json::parse("{ }");
        }
    }
}

void Settings::Save() {
    if (this->mode == ModeReadOnly) {
        throw std::runtime_error("cannot save a ModeReadOnly Setting!");
    }
    this->Flush();
}

bool Settings::Flush() {
    std::lock_guard lock(this->mutex);
    if (this->mode == ModeReadOnly) {
        return false;
    }
    return stringToFile(FILENAME(this->component), this->json.dump(2));
}

namespace {
    Settings& settingStore(mc_settings_scope scope) {
        if (scope == MC_SETTINGS_APPLICATION) {
            static const auto settings = Settings::ForComponent(prefs::components::Settings);
            return *settings;
        }

        if (scope == MC_SETTINGS_SESSION) {
            static const auto session = Settings::ForComponent(prefs::components::Session);
            return *session;
        }

        static const auto hotkeys = Settings::ForComponent("hotkeys", Settings::ModeReadWrite);
        return *hotkeys;
    }

    std::string settingText(mc_string_view view) {
        return std::string(view.data ? view.data : "", view.length);
    }

}

json Settings::EncodeValue(const mc_setting_value& value) {
    switch (value.type) {
        case MC_SETTING_BOOLEAN:
            return value.data.boolean;
        case MC_SETTING_INTEGER:
            return value.data.integer;
        case MC_SETTING_NUMBER:
            return value.data.number;
        case MC_SETTING_TEXT:
            return settingText(value.data.text);
    }
    return nullptr;
}

mc_setting_value Settings::ReadValue(
    const std::string& key, mc_setting_value value, std::string& text)
{
    switch (value.type) {
        case MC_SETTING_BOOLEAN:
            value.data.boolean = this->GetBool(key, value.data.boolean);
            break;
        case MC_SETTING_INTEGER:
            value.data.integer = this->GetInt(key, value.data.integer);
            break;
        case MC_SETTING_NUMBER:
            value.data.number = this->GetDouble(key, value.data.number);
            break;
        case MC_SETTING_TEXT:
            text = this->GetString(key, settingText(value.data.text));
            value.data.text = { text.data(), text.size() };
            break;
    }
    return value;
}

mc_result MC_CALL mc_settings_read(mc_settings_scope scope, mc_string_view key,
    const mc_setting_value* fallback, mc_setting_read_fn read, void* data)
{
    if (!ffi::valid_setting_scope(scope) || !ffi::valid_text(key, false) || !read ||
        !fallback || !ffi::valid_setting_value(*fallback))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return ffi::exception_barrier(nullptr, [&] {
        auto& store = settingStore(scope);
        std::string text;
        const auto value = store.ReadValue(settingText(key), *fallback, text);
        read(data, &value);
        return MC_OK;
    });
}

mc_result MC_CALL mc_settings_write(mc_settings_scope scope, const mc_setting_entry* entries, size_t count) {
    if (!ffi::valid_setting_scope(scope) || !ffi::valid_setting_entries(entries, count)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (!count) {
        return MC_OK;
    }

    return ffi::exception_barrier(nullptr, [&] {
        auto values = json::object();
        for (size_t i = 0; i < count; ++i) {
            values[settingText(entries[i].key)] = Settings::EncodeValue(entries[i].value);
        }
        settingStore(scope).SetValues(values);
        return MC_OK;
    });
}

mc_result MC_CALL mc_settings_save(mc_settings_scope scope) {
    if (!ffi::valid_setting_scope(scope)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        return settingStore(scope).Flush() ? MC_OK : MC_ERROR_IO;
    });
}

mc_result MC_CALL mc_settings_list_keys(mc_settings_scope scope, mc_setting_key_fn read, void* data) {
    if (!ffi::valid_setting_scope(scope) || !read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        std::vector<std::string> keys;
        settingStore(scope).GetKeys(keys);
        for (const auto& key : keys) {
            read(data, { key.data(), key.size() });
        }
        return MC_OK;
    });
}
