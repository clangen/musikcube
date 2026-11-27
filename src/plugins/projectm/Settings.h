#pragma once

#include <musikcore/plugin/settings.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <vector>

/* renderer choices survive window/helper replacement. paths are relative to
the collection so bundled presets also survive moving the application. */
struct ProjectMSettings {
    bool automatic { true };
    bool controlsVisible { true };
    bool fullscreen { false };
    bool reduceTransitionWork { true };
    float duration { 30.0f };
    float gain { 1.0f };
    int32_t width { 960 };
    int32_t height { 540 };
    std::array<char, 4096> directory {};
    std::array<char, 4096> preset {};

    bool operator==(const ProjectMSettings&) const = default;
};

inline void NormalizeProjectMSettings(ProjectMSettings& settings) {
    settings.duration = std::isfinite(settings.duration) ? std::clamp(settings.duration, 5.0f, 300.0f) : 30.0f;
    settings.gain = std::isfinite(settings.gain) ? std::clamp(settings.gain, 1.0f, 8.0f) : 1.0f;
    settings.width = std::clamp(settings.width, 320, 8192);
    settings.height = std::clamp(settings.height, 240, 8192);
    settings.directory.back() = 0;
    settings.preset.back() = 0;
}

inline mc_string_view ProjectMSettingKey(std::string_view key) {
    return { key.data(), key.size() };
}

inline ProjectMSettings LoadProjectMSettings(const mc_plugin_settings_service* store) {
    ProjectMSettings settings;
    if (!store) {
        return settings;
    }
    auto boolean = [&](const char* key, bool& value) {
        if (store->read_boolean) {
            store->read_boolean(store->context, ProjectMSettingKey(key), value, &value);
        }
    };
    auto number = [&](const char* key, float& value) {
        double saved = value;
        if (store->read_number && store->read_number(store->context, ProjectMSettingKey(key), value, &saved) == MC_OK) {
            value = static_cast<float>(saved);
        }
    };
    auto integer = [&](const char* key, int32_t minimum, int32_t& value) {
        if (store->read_integer) {
            const mc_integer_setting field {
                .key = ProjectMSettingKey(key),
                .default_value = value,
                .minimum = minimum,
                .maximum = 8192,
            };
            store->read_integer(store->context, &field, &value);
        }
    };
    auto text = [&](const char* key, std::array<char, 4096>& value) {
        if (store->read_text) {
            store->read_text(
                store->context, ProjectMSettingKey(key), { "", 0 },
                [](void* user, mc_string_view text) {
                    auto& target = *static_cast<std::array<char, 4096>*>(user);
                    if (text.length < target.size()) {
                        if (text.length) {
                            std::memcpy(target.data(), text.data, text.length);
                        }
                        target[text.length] = 0;
                    }
                },
                &value);
        }
    };
    boolean("automatic", settings.automatic);
    boolean("controls_visible", settings.controlsVisible);
    boolean("fullscreen", settings.fullscreen);
    boolean("reduce_transition_work", settings.reduceTransitionWork);
    number("duration_seconds", settings.duration);
    number("audio_gain", settings.gain);
    integer("window_width", 320, settings.width);
    integer("window_height", 240, settings.height);
    text("preset_directory", settings.directory);
    text("preset", settings.preset);
    NormalizeProjectMSettings(settings);
    return settings;
}

inline mc_result SaveProjectMSettings(const mc_plugin_settings_service* store, const ProjectMSettings& settings) {
    if (!store) {
        return MC_OK;
    }
    if (!store->write_values) {
        return MC_ERROR_UNSUPPORTED;
    }
    const mc_setting_entry values[] {
        {
            .key = ProjectMSettingKey("automatic"),
            .value = {
                .type = MC_SETTING_BOOLEAN,
                .data = {
                    .boolean = settings.automatic,
                },
            },
        },
        {
            .key = ProjectMSettingKey("controls_visible"),
            .value = {
                .type = MC_SETTING_BOOLEAN,
                .data = {
                    .boolean = settings.controlsVisible,
                },
            },
        },
        {
            .key = ProjectMSettingKey("fullscreen"),
            .value = {
                .type = MC_SETTING_BOOLEAN,
                .data = {
                    .boolean = settings.fullscreen,
                },
            },
        },
        {
            .key = ProjectMSettingKey("reduce_transition_work"),
            .value = {
                .type = MC_SETTING_BOOLEAN,
                .data = {
                    .boolean = settings.reduceTransitionWork,
                },
            },
        },
        {
            .key = ProjectMSettingKey("duration_seconds"),
            .value = {
                .type = MC_SETTING_NUMBER,
                .data = {
                    .number = settings.duration,
                },
            },
        },
        {
            .key = ProjectMSettingKey("audio_gain"),
            .value = {
                .type = MC_SETTING_NUMBER,
                .data = {
                    .number = settings.gain,
                },
            },
        },
        {
            .key = ProjectMSettingKey("window_width"),
            .value = {
                .type = MC_SETTING_INTEGER,
                .data = {
                    .integer = settings.width,
                },
            },
        },
        {
            .key = ProjectMSettingKey("window_height"),
            .value = {
                .type = MC_SETTING_INTEGER,
                .data = {
                    .integer = settings.height,
                },
            },
        },
        {
            .key = ProjectMSettingKey("preset_directory"),
            .value = {
                .type = MC_SETTING_TEXT,
                .data = {
                    .text = ProjectMSettingKey(settings.directory.data()),
                },
            },
        },
        {
            .key = ProjectMSettingKey("preset"),
            .value = {
                .type = MC_SETTING_TEXT,
                .data = {
                    .text = ProjectMSettingKey(settings.preset.data()),
                },
            },
        },
    };
    return store->write_values(store->context, values, std::size(values));
}

inline size_t FindProjectMPreset(
    const std::vector<std::string>& presets, const std::filesystem::path& directory, const char* selected) {
    const auto desired = std::filesystem::absolute(directory / selected).lexically_normal();
    for (size_t i = 0; i < presets.size(); ++i) {
        if (std::filesystem::absolute(presets[i]).lexically_normal() == desired) {
            return i;
        }
    }
    return 0;
}
