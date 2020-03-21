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

#include "pch.hpp"

#include <core/support/Preferences.h>
#include <core/support/Common.h>
#include <core/plugin/PluginFactory.h>

#include <unordered_map>

using nlohmann::json;
using namespace musik::core;

static std::unordered_map<std::string, std::weak_ptr<Preferences> > componentCache;
static std::unordered_map<std::string, std::shared_ptr<Preferences> > pluginCache;
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

    size_t written = fwrite(str.c_str(), str.size(), 1, f);
    fclose(f);
    return (written == str.size());
}

static std::string pluginFilename(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    name = "plugin_" + name; /* filename = nowhitespace(tolower(name)).json */
    return name;
}

void Preferences::LoadPluginPreferences() {
    typedef void(*SetPreferencesPlugin)(musik::core::sdk::IPreferences*);

    PluginFactory::Instance().QueryFunction<SetPreferencesPlugin>(
        "SetPreferences",
        [](musik::core::sdk::IPlugin* plugin, SetPreferencesPlugin func) {
            auto prefs = Preferences::ForPlugin(plugin->Name());
            func(prefs.get());
        });
}

void Preferences::SavePluginPreferences() {
    pluginCache.clear(); /* dtors will save */
}

std::shared_ptr<Preferences> Preferences::ForPlugin(const std::string& pluginName) {
    std::string name = pluginFilename(pluginName);

    if (pluginCache.find(name) == pluginCache.end()) {
        pluginCache[name] = std::shared_ptr<Preferences>(
            new Preferences(name, Preferences::ModeAutoSave));
    }

    return pluginCache[name];
}

musik::core::sdk::IPreferences* Preferences::Unmanaged(const std::string& name) {
    if (!name.size()) {
        return new Preferences(name, ModeTransient);
    }

    return Preferences::ForPlugin("unmanaged_" + name).get();
}

std::shared_ptr<Preferences> Preferences::ForComponent(
    const std::string& c, Preferences::Mode mode)
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

    std::shared_ptr<Preferences> prefs(new Preferences(c, mode));
    componentCache[key] = prefs;
    return prefs;
}

Preferences::Preferences(const std::string& component, Mode mode) {
    this->mode = mode;
    this->component = component;
    this->Load();
}

Preferences::~Preferences() {
    if (this->mode == ModeAutoSave) {
        this->Save();
    }
}

void Preferences::Release() {
    if (this->mode == ModeTransient) {
        delete this;
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

bool Preferences::GetBool(const std::string& key, bool defaultValue) {
    const char* p = key.c_str();
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

int Preferences::GetInt(const std::string& key, int defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

double Preferences::GetDouble(const std::string& key, double defaultValue) {
    std::unique_lock<std::mutex> lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

std::string Preferences::GetString(const std::string& key, const std::string& defaultValue) {
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

void Preferences::SetBool(const std::string& key, bool value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Preferences::SetInt(const std::string& key, int value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Preferences::SetDouble(const std::string& key, double value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Preferences::SetString(const std::string& key, const char* value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    json[key] = value;
}

void Preferences::GetKeys(std::vector<std::string>& target) {
    auto it = json.begin();
    for (; it != json.end(); it++) {
        target.push_back(it.key());
    }
}

bool Preferences::HasKey(const std::string& key) {
    return json.find(key) != json.end();
}

void Preferences::Remove(const std::string& key) {
    auto it = json.find(key);
    if (it != json.end()) {
        json.erase(it);
    }
}

void Preferences::Load() {
    std::string str = fileToString(FILENAME(this->component));
    if (str.size()) {
        try {
            this->json = json::parse(str);
        }
        catch (...) {
            std::cerr << "error loading " << this->component << ".json";
            this->json = json::parse("{ }");
        }
    }
}

void Preferences::Save() {
    if (this->mode == ModeReadOnly) {
        throw std::runtime_error("cannot save a ModeReadOnly Preference!");
    }
    else if (this->mode != ModeTransient) {
        stringToFile(FILENAME(this->component), this->json.dump(2));
    }
}

/* SDK IPreferences interface */

bool Preferences::GetBool(const char* key, bool defaultValue) {
    return this->GetBool(std::string(key), defaultValue);
}

int Preferences::GetInt(const char* key, int defaultValue) {
    return this->GetInt(std::string(key), defaultValue);
}

double Preferences::GetDouble(const char* key, double defaultValue) {
    return this->GetDouble(std::string(key), defaultValue);
}

int Preferences::GetString(const char* key, char* dst, size_t size, const char* defaultValue) {
    std::string value = this->GetString(std::string(key), defaultValue);
    return (int) CopyString(value, dst, size);
}

void Preferences::SetBool(const char* key, bool value) {
    this->SetBool(std::string(key), value);
}

void Preferences::SetInt(const char* key, int value) {
    this->SetInt(std::string(key), value);
}

void Preferences::SetDouble(const char* key, double value) {
    this->SetDouble(std::string(key), value);
}

void Preferences::SetString(const char* key, const char* value) {
    this->SetString(std::string(key), value);
}