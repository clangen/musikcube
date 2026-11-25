#include "pch.hpp"
#include <musikcore/plugins.hpp>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/consumer/preferences-shape.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/sdk/Filesystem.h>
#include <algorithm>
#include <filesystem>
#include <string_view>

using namespace musik::core;
using musik::core::sdk::fs::pathToUtf8;

namespace {
    std::vector<api::PluginInfo> pluginCatalog() {
        const auto settings = Preferences::ForComponent(prefs::components::Plugins);
        std::vector<api::PluginInfo> plugins;
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

    std::string_view pluginText(mc_string_view text) {
        return std::string_view(text.data ? text.data : "", text.length);
    }
}

mc_result MC_CALL mc_plugins_list(mc_plugin_info_fn read, void* data) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const auto thread = plugin::ValidateCallingThread();
    if (thread != MC_OK) {
        return thread;
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
    const auto thread = plugin::ValidateCallingThread();
    if (thread != MC_OK) {
        return thread;
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
    const auto thread = plugin::ValidateCallingThread();
    if (thread != MC_OK) {
        return thread;
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
        const auto catalog = pluginCatalog();
        return std::ranges::any_of(catalog, [&](const auto& item) { return item.id == pluginId; })
            ? MC_ERROR_UNSUPPORTED : MC_ERROR_NOT_FOUND;
    });
}

mc_result MC_CALL mc_plugin_write_settings(mc_string_view id, const mc_preference_entry* entries, size_t count) {
    if (!ffi::valid_text(id, false) || !ffi::valid_preference_entries(entries, count)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const auto thread = plugin::ValidateCallingThread();
    if (thread != MC_OK) {
        return thread;
    }
    return ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const auto pluginId = pluginText(id);
        const auto configuration = plugin::FindConfiguration(pluginId);
        nlohmann::json updates = nlohmann::json::object();
        if (configuration) {
            const auto catalog = plugin::ListPlugins();
            const auto item = std::ranges::find(catalog, pluginId, &ffi::PluginRegistry::PluginInfo::id);
            if (item == catalog.end()) {
                return MC_ERROR_NOT_FOUND;
            }
            const auto valid = configuration.Invoke([&](const mc_configuration_api& schema) -> mc_result {
                for (size_t index = 0; index < count; ++index) {
                    const auto& entry = entries[index];
                    const auto key = pluginText(entry.key);
                    if (updates.contains(std::string(key))) {
                        return MC_ERROR_INVALID_ARGUMENT;
                    }
                    bool found = false;
                    for (size_t i = 0; i < schema.integer_count; ++i) {
                        const auto& field = schema.integers[i];
                        if (pluginText(field.key) == key) {
                            if (entry.value.type != MC_PREFERENCE_INTEGER || entry.value.data.integer < field.minimum ||
                                entry.value.data.integer > field.maximum)
                            {
                                return MC_ERROR_INVALID_ARGUMENT;
                            }
                            found = true;
                            break;
                        }
                    }
                    for (size_t i = 0; !found && i < schema.boolean_count; ++i) {
                        if (pluginText(schema.booleans[i].key) == key) {
                            if (entry.value.type != MC_PREFERENCE_BOOLEAN) {
                                return MC_ERROR_INVALID_ARGUMENT;
                            }
                            found = true;
                        }
                    }
                    for (size_t i = 0; !found && i < schema.number_count; ++i) {
                        const auto& field = schema.numbers[i];
                        if (pluginText(field.key) == key) {
                            if (entry.value.type != MC_PREFERENCE_NUMBER || entry.value.data.number < field.minimum ||
                                entry.value.data.number > field.maximum)
                            {
                                return MC_ERROR_INVALID_ARGUMENT;
                            }
                            found = true;
                        }
                    }
                    for (size_t i = 0; !found && i < schema.text_count; ++i) {
                        if (pluginText(schema.texts[i].key) == key) {
                            if (entry.value.type != MC_PREFERENCE_TEXT) {
                                return MC_ERROR_INVALID_ARGUMENT;
                            }
                            found = true;
                        }
                    }
                    if (!found) {
                        return MC_ERROR_NOT_FOUND;
                    }
                    updates[std::string(key)] = Preferences::EncodeValue(entry.value);
                }
                return MC_OK;
            });
            if (valid != MC_OK) {
                return valid;
            }
            return !count || Preferences::ForPlugin(item->name)->SetValues(updates, true) ? MC_OK : MC_ERROR_IO;
        }
        const auto catalog = pluginCatalog();
        return std::ranges::any_of(catalog, [&](const auto& item) { return item.id == pluginId; })
            ? MC_ERROR_UNSUPPORTED : MC_ERROR_NOT_FOUND;
    });
}

mc_result MC_CALL mc_plugin_write_setting(mc_string_view id, const mc_preference_entry* entry) {
    if (!entry) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return mc_plugin_write_settings(id, entry, 1);
}

mc_result MC_CALL mc_plugin_apply_settings(mc_string_view id) {
    if (!ffi::valid_text(id, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        return plugin::ApplySettings(pluginText(id));
    });
}
