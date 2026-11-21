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
#include "musikcore/sdk/Filesystem.h"

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/ISchema.h>
#include <musikcore/plugins.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/consumer/preferences-shape.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>
#include <musikcore/config.h>
#include <musikcore/debug.h>
#include <musikcore/utfutil.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <string_view>

static const std::string TAG = "PluginFactory";
static std::mutex instanceMutex;

using musik::core::sdk::fs::pathToUtf8;

namespace fs = std::filesystem;

using namespace musik::core;

#ifdef WIN32
typedef musik::core::sdk::IPlugin* STDCALL(CallGetPlugin);
static void closeNativeHandle(void* dll) { FreeLibrary((HMODULE)dll); }
#else
typedef musik::core::sdk::IPlugin* (*CallGetPlugin)();
static void closeNativeHandle(void* dll) { dlclose(dll); }
#endif

PluginFactory& PluginFactory::Instance() {
    std::unique_lock<std::mutex> lock(instanceMutex);

    static PluginFactory* instance = NULL;

    if (instance == NULL) {
        instance = new PluginFactory();
    }

    return *instance;
}

PluginFactory::PluginFactory() {
    this->prefs = Preferences::ForComponent(prefs::components::Plugins);
    musik::debug::info(TAG, "loading plugins");
    this->LoadPlugins();
}

PluginFactory::~PluginFactory() {
    for (std::shared_ptr<Descriptor> plugin : this->plugins) {
        plugin->plugin->Release();
        closeNativeHandle(plugin->nativeHandle);
    }
    plugins.clear();
}

void PluginFactory::LoadPlugins() {
#ifdef WIN32
    {
        std::wstring wpath = u8to16(GetPluginDirectory());
        SetDllDirectory(wpath.c_str());
    }
#endif

    std::string pluginDir(GetPluginDirectory());
    fs::path dir(fs::u8path(pluginDir));

    try {
        fs::directory_iterator end;
        for (fs::directory_iterator file(dir); file != end; file++) {
            if (fs::is_regular_file(file->status())){
                std::string filename = pathToUtf8(fs::canonical(fs::path(file->path()).make_preferred()));
                std::shared_ptr<Descriptor> descriptor(new Descriptor());
                descriptor->filename = filename;
                descriptor->key = pathToUtf8(fs::path(fs::u8path(filename)).filename());
#ifdef WIN32
                /* if the file ends with ".dll", we'll try to load it*/
                if (filename.substr(filename.size() - 4) == ".dll") {
                    HMODULE dll = LoadLibrary(u8to16(filename).c_str());
                    if (dll != NULL) {
                        /* every plugin has a "GetPlugin" method. */
                        CallGetPlugin getPluginCall = (CallGetPlugin) GetProcAddress(dll, "GetPlugin");

                        if (getPluginCall) {
                            /* exists? check the version, and add it! */
                            auto plugin = getPluginCall();
                            if (plugin->SdkVersion() == musik::core::sdk::SdkVersion) {
                                descriptor->plugin = plugin;
                                descriptor->nativeHandle = dll;
                                this->plugins.push_back(descriptor);
                                musik::debug::info(TAG, "loaded: " + filename);
                            }
                            else {
                                musik::debug::error(TAG, "plugin with invalid version: " + filename);
                                FreeLibrary(dll);
                            }
                        }
                        else {
                            /* otherwise, free and move on */
                            musik::debug::error(TAG, "plugin entry point not found: " + filename);
                            FreeLibrary(dll);
                        }
                    }
                }
#else
    #ifdef __APPLE__
                if (filename.substr(filename.size() - 6) == ".dylib") {
                    int openFlags = RTLD_LOCAL;
    #else
                if (filename.substr(filename.size() - 3) == ".so") {
                    int openFlags = RTLD_NOW;
    #endif
                    void* dll = NULL;

                    try {
                        dll = dlopen(filename.c_str(), openFlags);
                    }
                    catch (...) {
                        musik::debug::error(TAG, "exception while loading plugin " + filename);
                        continue;
                    }

                    if (!dll) {
                        char *err = dlerror();

                        musik::debug::error(TAG, u8fmt("exception while loading plugin %s: %s", filename.c_str(), err).c_str());
                    }
                    else {
                        CallGetPlugin getPluginCall;
                        *(void **)(&getPluginCall) = dlsym(dll, "GetPlugin");

                        if (getPluginCall) {
                            auto plugin = getPluginCall();
                            if (plugin->SdkVersion() == musik::core::sdk::SdkVersion) {
                                musik::debug::info(TAG, "loaded: " + filename);
                                descriptor->plugin = getPluginCall();
                                descriptor->nativeHandle = dll;
                                this->plugins.push_back(descriptor);
                            }
                            else {
                                dlclose(dll);
                            }
                        }
                        else {
                            dlclose(dll);
                        }
                    }
                }
#endif
            }
        }
    }
    catch(...) {
    }
}

namespace {
    using musik::core::sdk::IPlugin;
    using musik::core::sdk::ISchema;

    struct PluginDetails {
        std::string id;
        std::string name;
        std::string version;
        std::string filename;
        bool enabled;
    };

    std::vector<PluginDetails> pluginCatalog() {
        const auto settings = Preferences::ForComponent(prefs::components::Plugins);
        std::vector<PluginDetails> plugins;
        PluginFactory::Instance().QueryInterface<IPlugin, PluginFactory::ReleaseDeleter<IPlugin>>(
            "GetPlugin", [&](IPlugin*, const std::shared_ptr<IPlugin>& plugin, const std::string& path) {
                const auto filename = pathToUtf8(fs::u8path(path).filename());
                plugins.push_back({
                    plugin->Guid(), plugin->Name(), plugin->Version(),
                    filename, settings->GetBool(filename, true)
                });
            });
        return plugins;
    }

    mc_string_view pluginView(std::string_view text) {
        return { text.data(), text.size() };
    }

    mc_string_view pluginView(const char* text) {
        return text ? pluginView(std::string_view(text)) : mc_string_view {};
    }

    std::string_view pluginText(mc_string_view text) {
        return std::string_view(text.data ? text.data : "", text.length);
    }

    std::shared_ptr<ISchema> pluginSchema(std::string_view id) {
        std::shared_ptr<ISchema> schema;
        PluginFactory::Instance().QueryInterface<ISchema, PluginFactory::ReleaseDeleter<ISchema>>(
            "GetSchema", [&](IPlugin* plugin, const std::shared_ptr<ISchema>& found, const std::string&) {
                if (plugin->Guid() == id) {
                    schema = found;
                }
            });
        return schema;
    }

    struct SettingDescription {
        mc_preference_value fallback {};
        double minimum = 0.0;
        double maximum = 0.0;
        int32_t precision = 0;
        const char** choices = nullptr;
        size_t choiceCount = 0;
    };

    SettingDescription describeSetting(const ISchema::Entry& entry) {
        SettingDescription result;
        switch (entry.type) {
            case ISchema::Type::Bool: {
                const auto& item = reinterpret_cast<const ISchema::BoolEntry&>(entry);
                result.fallback.type = MC_PREFERENCE_BOOLEAN;
                result.fallback.data.boolean = item.defaultValue;
            }
            break;
            case ISchema::Type::Int: {
                const auto& item = reinterpret_cast<const ISchema::IntEntry&>(entry);
                result.fallback.type = MC_PREFERENCE_INTEGER;
                result.fallback.data.integer = item.defaultValue;
                result.minimum = item.minValue;
                result.maximum = item.maxValue;
            }
            break;
            case ISchema::Type::Double: {
                const auto& item = reinterpret_cast<const ISchema::DoubleEntry&>(entry);
                result.fallback.type = MC_PREFERENCE_NUMBER;
                result.fallback.data.number = item.defaultValue;
                result.minimum = item.minValue == std::numeric_limits<double>::min()
                    ? std::numeric_limits<double>::lowest()
                    : item.minValue;
                result.maximum = item.maxValue;
                result.precision = item.precision;
            }
            break;
            case ISchema::Type::String: {
                const auto& item = reinterpret_cast<const ISchema::StringEntry&>(entry);
                result.fallback.type = MC_PREFERENCE_TEXT;
                result.fallback.data.text = pluginView(item.defaultValue);
            }
            break;
            case ISchema::Type::Enum: {
                const auto& item = reinterpret_cast<const ISchema::EnumEntry&>(entry);
                result.fallback.type = MC_PREFERENCE_TEXT;
                result.fallback.data.text = pluginView(item.defaultValue);
                result.choices = item.values;
                result.choiceCount = item.count;
            }
            break;
        }
        return result;
    }

    bool settingValueAllowed(const SettingDescription& field, const mc_preference_value& value) {
        if (field.fallback.type != value.type) {
            return false;
        }
        switch (value.type) {
            case MC_PREFERENCE_BOOLEAN:
                return true;
            case MC_PREFERENCE_INTEGER:
                return value.data.integer >= field.minimum && value.data.integer <= field.maximum;
            case MC_PREFERENCE_NUMBER:
                return value.data.number >= field.minimum && value.data.number <= field.maximum;
            case MC_PREFERENCE_TEXT:
                return !field.choices || std::find(field.choices, field.choices + field.choiceCount,
                    pluginText(value.data.text)) != field.choices + field.choiceCount;
        }
        return false;
    }
}

mc_result MC_CALL mc_plugins_list(mc_plugin_info_fn read, void* data) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        for (const auto& plugin : pluginCatalog()) {
            const mc_plugin_info row {
                pluginView(plugin.id), pluginView(plugin.name), pluginView(plugin.version),
                pluginView(plugin.filename), plugin.enabled
            };
            read(data, &row);
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_plugin_set_enabled(mc_string_view filename, bool enabled) {
    if (!ffi::valid_text(filename, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        auto key = pluginText(filename);
        const auto plugins = pluginCatalog();
        if (!std::ranges::any_of(plugins, [key](const auto& plugin) { return plugin.filename == key; })) {
            return MC_ERROR_NOT_FOUND;
        }
        auto prefs = Preferences::ForComponent(prefs::components::Plugins);
        return prefs->SetValues({ { std::string(key), enabled } }, true) ? MC_OK : MC_ERROR_IO;
    });
}

mc_result MC_CALL mc_plugin_read_settings(mc_string_view id, mc_plugin_setting_fn read, void* data) {
    if (!read || !ffi::valid_text(id, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        const auto pluginId = pluginText(id);
        const auto plugin = PluginFactory::Instance().QueryGuid(std::string(pluginId));
        if (!plugin) {
            return MC_ERROR_NOT_FOUND;
        }
        const auto schema = pluginSchema(pluginId);
        if (!schema) {
            return MC_ERROR_UNSUPPORTED;
        }

        const auto settings = Preferences::ForPlugin(plugin->Name());
        for (size_t i = 0; i < schema->Count(); ++i) {
            const auto entry = schema->At(i);
            if (!entry || !entry->name) {
                return MC_ERROR_PLUGIN;
            }

            const auto description = describeSetting(*entry);
            std::string text;
            const auto value = settings->ReadValue(entry->name, description.fallback, text);
            if (description.choiceCount && !description.choices) {
                return MC_ERROR_PLUGIN;
            }

            std::vector<mc_string_view> choices;
            choices.reserve(description.choiceCount);
            for (size_t index = 0; index < description.choiceCount; ++index) {
                choices.emplace_back(pluginView(description.choices[index]));
            }
            const mc_plugin_setting row {
                pluginView(entry->name), value, description.minimum, description.maximum,
                description.precision, choices.data(), choices.size(),
                entry->type == ISchema::Type::String && description.fallback.data.text.length != 0
            };
            read(data, &row);
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_plugin_write_setting(mc_string_view id, const mc_preference_entry* entry) {
    if (!entry || !ffi::valid_text(id, false) || !ffi::valid_preference_entries(entry, 1)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        const auto pluginId = pluginText(id);
        const auto plugin = PluginFactory::Instance().QueryGuid(std::string(pluginId));
        if (!plugin) {
            return MC_ERROR_NOT_FOUND;
        }
        const auto schema = pluginSchema(pluginId);
        if (!schema) {
            return MC_ERROR_UNSUPPORTED;
        }

        const auto key = pluginText(entry->key);
        for (size_t i = 0; i < schema->Count(); ++i) {
            const auto field = schema->At(i);
            if (!field || !field->name) {
                return MC_ERROR_PLUGIN;
            }
            if (field->name != key) {
                continue;
            }
            if (!settingValueAllowed(describeSetting(*field), entry->value)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }

            const auto settings = Preferences::ForPlugin(plugin->Name());
            auto stored = Preferences::EncodeValue(entry->value);
            return settings->SetValues({ { std::string(key), std::move(stored) } }, true)
                ? MC_OK
                : MC_ERROR_IO;
        }
        return MC_ERROR_NOT_FOUND;
    });
}
