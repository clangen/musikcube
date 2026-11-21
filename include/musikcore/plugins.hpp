#ifndef MUSIKCORE_PLUGINS_HPP
#define MUSIKCORE_PLUGINS_HPP

#include <musikcore/plugins.h>
#include <musikcore/preferences.hpp>

#include <vector>

namespace musik { namespace core { namespace api {
    /* Owns one installed plugin's metadata. Copyable and movable. */
    struct PluginInfo {
        std::string id;
        std::string name;
        std::string version;
        std::string filename;
        bool enabled;
    };

    inline std::vector<PluginInfo> ListPlugins() {
        std::vector<PluginInfo> result;
        const auto status = mc_plugins_list([](void* data, const mc_plugin_info* plugin) {
            static_cast<std::vector<PluginInfo>*>(data)->push_back({
                std::string(AsStringView(plugin->id)), std::string(AsStringView(plugin->name)),
                std::string(AsStringView(plugin->version)), std::string(AsStringView(plugin->filename)),
                plugin->enabled
            });
        }, &result);
        if (status != MC_OK) {
            throw Error(status);
        }
        return result;
    }

    inline mc_result SetPluginEnabled(std::string_view filename, bool enabled) {
        return mc_plugin_set_enabled({ filename.data(), filename.size() }, enabled);
    }

    inline std::optional<std::vector<PreferenceField>> ReadPluginSettings(std::string_view id) {
        std::vector<PreferenceField> fields;
        const auto status = mc_plugin_read_settings({ id.data(), id.size() },
            [](void* data, const mc_plugin_setting* setting) {
                PreferenceField field;
                field.key = AsStringView(setting->key);
                field.minimum = setting->minimum;
                field.maximum = setting->maximum;
                field.precision = setting->precision;
                field.allowEmpty = setting->allow_empty;
                switch (setting->value.type) {
                    case MC_PREFERENCE_BOOLEAN:
                        field.value = setting->value.data.boolean;
                        break;
                    case MC_PREFERENCE_INTEGER:
                        field.value = setting->value.data.integer;
                        break;
                    case MC_PREFERENCE_NUMBER:
                        field.value = setting->value.data.number;
                        break;
                    case MC_PREFERENCE_TEXT:
                        field.value = std::string(AsStringView(setting->value.data.text));
                        break;
                }
                field.choices.reserve(setting->choice_count);
                for (size_t i = 0; i < setting->choice_count; ++i) {
                    field.choices.emplace_back(AsStringView(setting->choices[i]));
                }
                static_cast<std::vector<PreferenceField>*>(data)->push_back(std::move(field));
            }, &fields);
        if (status == MC_ERROR_NOT_FOUND || status == MC_ERROR_UNSUPPORTED) {
            return std::nullopt;
        }
        if (status != MC_OK) {
            throw Error(status);
        }
        return fields;
    }

    inline mc_result WritePluginSetting(std::string_view id, std::string_view key, const PreferenceValue& value) {
        const auto entry = std::visit([key](const auto& data) { return Preference(key, data); }, value);
        return mc_plugin_write_setting({ id.data(), id.size() }, &entry);
    }
} } }

#endif
