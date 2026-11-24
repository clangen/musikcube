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
#include <musikcore/plugins.hpp>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/consumer/preferences-shape.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>
#include <musikcore/config.h>
#include <musikcore/debug.h>
#include <common/Utf8.h>
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

PluginFactory& PluginFactory::Instance(const std::string& directory) {
    std::unique_lock<std::mutex> lock(instanceMutex);

    static PluginFactory* instance = NULL;

    if (instance == NULL) {
        auto path = musik::core::sdk::fs::pathFromUtf8(directory.empty() ? GetPluginDirectory() : directory);
        if (!directory.empty() && !fs::is_directory(path)) {
            throw std::invalid_argument("Plugin directory does not exist");
        }
        instance = new PluginFactory(pathToUtf8(fs::weakly_canonical(path)));
    }
    else if (!directory.empty() &&
        fs::weakly_canonical(musik::core::sdk::fs::pathFromUtf8(directory)) != musik::core::sdk::fs::pathFromUtf8(instance->directory))
    {
        throw std::invalid_argument("Plugins have already been discovered in another directory");
    }

    return *instance;
}

PluginFactory::PluginFactory(std::string directory)
    : directory(std::move(directory)) {
    this->prefs = Preferences::ForComponent(prefs::components::Plugins);
    musik::debug::info(TAG, "loading plugins");
    this->LoadPlugins();
}

bool PluginFactory::IsEnabled(const Descriptor& descriptor) const {
    if (descriptor.parentKey.empty()) {
        return this->prefs->GetBool(descriptor.key, true);
    }
    /* reading the catalog must leave an unset companion free to follow its parent. */
    return this->prefs->Lookup({ descriptor.key, descriptor.parentKey }, true);
}

std::vector<api::PluginInfo> PluginFactory::ListPlugins() {
    std::lock_guard lock(this->mutex);
    std::vector<api::PluginInfo> plugins;
    for (const auto& descriptor : this->plugins) {
        const auto plugin = descriptor->plugin;
        plugins.push_back({
            .id = plugin->Guid(),
            .name = plugin->Name(),
            .version = plugin->Version(),
            .filename = descriptor->key,
            .enabled = IsEnabled(*descriptor),
        });
    }
    return plugins;
}

void PluginFactory::BeginSession() {
    std::lock_guard lock(this->mutex);
    for (const auto& descriptor : this->plugins) {
        descriptor->session = this->IsEnabled(*descriptor);
    }
}

void PluginFactory::EndSession() {
    std::lock_guard lock(this->mutex);
    for (const auto& descriptor : this->plugins) {
        descriptor->session = false;
    }
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
        std::wstring wpath = u8to16(this->directory);
        SetDllDirectory(wpath.c_str());
    }
#endif

    fs::path dir(musik::core::sdk::fs::pathFromUtf8(this->directory));

    try {
        fs::directory_iterator end;
        for (fs::directory_iterator file(dir); file != end; file++) {
            if (fs::is_regular_file(file->status())){
                std::string filename = pathToUtf8(fs::canonical(fs::path(file->path()).make_preferred()));
                std::shared_ptr<Descriptor> descriptor(new Descriptor());
                descriptor->filename = filename;
                descriptor->key = pathToUtf8(fs::path(musik::core::sdk::fs::pathFromUtf8(filename)).filename());
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
                                plugin->Release();
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
                                descriptor->plugin = plugin;
                                descriptor->nativeHandle = dll;
                                this->plugins.push_back(descriptor);
                            }
                            else {
                                plugin->Release();
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
    /* a companion may name the plugin whose enable setting supplies its default.
    copy the name once while the module is loaded. */
    for (const auto& descriptor : this->plugins) {
        using ParentFilename = const char* (*)();
#ifdef WIN32
        auto parent = reinterpret_cast<ParentFilename>(
            GetProcAddress(static_cast<HMODULE>(descriptor->nativeHandle), "GetParentPluginFilename"));
#else
        auto parent = reinterpret_cast<ParentFilename>(dlsym(descriptor->nativeHandle, "GetParentPluginFilename"));
#endif
        if (parent) {
            if (const char* filename = parent()) {
                descriptor->parentKey = filename;
            }
        }
    }
}

namespace {
    using musik::core::sdk::IPlugin;
    using musik::core::sdk::ISchema;

    std::vector<api::PluginInfo> pluginCatalog() {
        const auto settings = Preferences::ForComponent(prefs::components::Plugins);
        auto plugins = PluginFactory::Instance().ListPlugins();
        for (const auto& plugin : musik::core::plugin::ListPlugins()) {
            const auto filename = pathToUtf8(musik::core::sdk::fs::pathFromUtf8(plugin.filename).filename());
            const bool enabled = settings->GetBool(filename, true);
            plugins.push_back({ plugin.id, plugin.name, plugin.version, filename,
                enabled });
        }
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
    return ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const auto pluginId = pluginText(id);
        const auto configuration = plugin::FindConfiguration(pluginId);
        if (configuration) {
            const auto catalog = plugin::ListPlugins();
            const auto item = std::ranges::find(catalog, pluginId, &ffi::PluginRegistry::PluginInfo::id);
            if (item == catalog.end()) {
                return MC_ERROR_NOT_FOUND;
            }
            auto preferences = plugin::ReadSettingsService(item->name);
            return configuration.Invoke([&](const mc_configuration_api& schema) -> mc_result {
                for (size_t i = 0; i < schema.integer_count; ++i) {
                    const auto& field = schema.integers[i];
                    mc_plugin_setting row {};
                    row.key = field.key;
                    row.minimum = field.minimum;
                    row.maximum = field.maximum;
                    row.value.type = MC_PREFERENCE_INTEGER;
                    const auto status = preferences.read_integer(preferences.context, &field, &row.value.data.integer);
                    if (status != MC_OK) {
                        return status;
                    }
                    read(data, &row);
                }
                for (size_t i = 0; i < schema.boolean_count; ++i) {
                    const auto& field = schema.booleans[i];
                    mc_plugin_setting row {};
                    row.key = field.key;
                    row.value.type = MC_PREFERENCE_BOOLEAN;
                    const auto status = preferences.read_boolean(
                        preferences.context, field.key, field.default_value, &row.value.data.boolean);
                    if (status != MC_OK) {
                        return status;
                    }
                    read(data, &row);
                }
                for (size_t i = 0; i < schema.number_count; ++i) {
                    const auto& field = schema.numbers[i];
                    mc_plugin_setting row {};
                    row.key = field.key;
                    row.minimum = field.minimum;
                    row.maximum = field.maximum;
                    row.precision = field.precision;
                    row.value.type = MC_PREFERENCE_NUMBER;
                    const auto status = preferences.read_number(
                        preferences.context, field.key, field.default_value, &row.value.data.number);
                    if (status != MC_OK) {
                        return status;
                    }
                    if (row.value.data.number < field.minimum || row.value.data.number > field.maximum) {
                        row.value.data.number = field.default_value;
                    }
                    read(data, &row);
                }
                const auto store = Preferences::ForPlugin(item->name);
                for (size_t i = 0; i < schema.text_count; ++i) {
                    const auto& field = schema.texts[i];
                    const std::string key(field.key.data, field.key.length);
                    const std::string fallback(field.default_value.data ? field.default_value.data : "",
                        field.default_value.length);
                    const auto value = store->GetString(key, fallback);
                    mc_plugin_setting row {};
                    row.key = field.key;
                    row.value.type = MC_PREFERENCE_TEXT;
                    row.value.data.text = { value.data(), value.size() };
                    row.allow_empty = !fallback.empty();
                    read(data, &row);
                }
                return MC_OK;
            });
        }
        const auto plugin = PluginFactory::Instance().QueryGuid(std::string(pluginId));
        if (!plugin) {
            const auto catalog = pluginCatalog();
            return std::ranges::any_of(catalog, [&](const auto& item) { return item.id == pluginId; })
                ? MC_ERROR_UNSUPPORTED : MC_ERROR_NOT_FOUND;
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
    return ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const auto pluginId = pluginText(id);
        const auto configuration = plugin::FindConfiguration(pluginId);
        if (configuration) {
            const auto catalog = plugin::ListPlugins();
            const auto item = std::ranges::find(catalog, pluginId, &ffi::PluginRegistry::PluginInfo::id);
            if (item == catalog.end()) {
                return MC_ERROR_NOT_FOUND;
            }
            return configuration.Invoke([&](const mc_configuration_api& schema) -> mc_result {
                for (size_t i = 0; i < schema.integer_count; ++i) {
                    const auto& field = schema.integers[i];
                    if (pluginText(field.key) != pluginText(entry->key)) {
                        continue;
                    }
                    if (entry->value.type != MC_PREFERENCE_INTEGER ||
                        entry->value.data.integer < field.minimum || entry->value.data.integer > field.maximum)
                    {
                        return MC_ERROR_INVALID_ARGUMENT;
                    }
                    const auto prefs = Preferences::ForPlugin(item->name);
                    return prefs->SetValues({ { std::string(pluginText(field.key)), entry->value.data.integer } }, true)
                        ? MC_OK
                        : MC_ERROR_IO;
                }
                const auto prefs = Preferences::ForPlugin(item->name);
                for (size_t i = 0; i < schema.boolean_count; ++i) {
                    if (pluginText(schema.booleans[i].key) == pluginText(entry->key)) {
                        if (entry->value.type != MC_PREFERENCE_BOOLEAN) {
                            return MC_ERROR_INVALID_ARGUMENT;
                        }
                        return prefs->SetValues({ { std::string(pluginText(entry->key)), entry->value.data.boolean } },
                            true) ? MC_OK : MC_ERROR_IO;
                    }
                }
                for (size_t i = 0; i < schema.number_count; ++i) {
                    const auto& field = schema.numbers[i];
                    if (pluginText(field.key) == pluginText(entry->key)) {
                        if (entry->value.type != MC_PREFERENCE_NUMBER || entry->value.data.number < field.minimum ||
                            entry->value.data.number > field.maximum)
                        {
                            return MC_ERROR_INVALID_ARGUMENT;
                        }
                        return prefs->SetValues({ { std::string(pluginText(entry->key)), entry->value.data.number } },
                            true) ? MC_OK : MC_ERROR_IO;
                    }
                }
                for (size_t i = 0; i < schema.text_count; ++i) {
                    if (pluginText(schema.texts[i].key) == pluginText(entry->key)) {
                        if (entry->value.type != MC_PREFERENCE_TEXT) {
                            return MC_ERROR_INVALID_ARGUMENT;
                        }
                        return prefs->SetValues({ { std::string(pluginText(entry->key)),
                            std::string(pluginText(entry->value.data.text)) } }, true) ? MC_OK : MC_ERROR_IO;
                    }
                }
                return MC_ERROR_NOT_FOUND;
            });
        }
        const auto plugin = PluginFactory::Instance().QueryGuid(std::string(pluginId));
        if (!plugin) {
            const auto catalog = pluginCatalog();
            return std::ranges::any_of(catalog, [&](const auto& item) { return item.id == pluginId; })
                ? MC_ERROR_UNSUPPORTED : MC_ERROR_NOT_FOUND;
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
