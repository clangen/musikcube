#pragma once

#include <musikcore/plugin/settings.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

/* reads the server's saved settings through the host-provided service. */
class ServerSettings {
public:
    explicit ServerSettings(const mc_plugin_settings_service& settings) : settings(settings) {}

    bool Boolean(std::string_view key, bool fallback = false) const {
        bool value = fallback;
        if (this->settings.read_boolean(this->settings.context, View(key), fallback, &value) != MC_OK) {
            throw std::runtime_error("could not read server setting");
        }
        return value;
    }

    int32_t Integer(std::string_view key, int32_t fallback = 0) const {
        const mc_integer_setting setting {
            .key = View(key),
            .default_value = fallback,
            .minimum = std::numeric_limits<int32_t>::min(),
            .maximum = std::numeric_limits<int32_t>::max(),
        };
        int32_t value = fallback;
        if (this->settings.read_integer(this->settings.context, &setting, &value) != MC_OK) {
            throw std::runtime_error("could not read server setting");
        }
        return value;
    }

    std::string Text(std::string_view key, std::string_view fallback = {}) const {
        struct Result {
            std::string value;
            bool failed { false };
        } result;
        const auto read = [](void* data, mc_string_view value) {
            auto& result = *static_cast<Result*>(data);
            try {
                result.value.assign(value.data ? value.data : "", value.length);
            }
            catch (...) {
                result.failed = true;
            }
        };
        if (this->settings.read_text(this->settings.context, View(key), View(fallback), read, &result) != MC_OK ||
            result.failed)
        {
            throw std::runtime_error("could not read server setting");
        }
        return result.value;
    }

private:
    static mc_string_view View(std::string_view text) {
        return {
            .data = text.data(),
            .length = text.size(),
        };
    }

    const mc_plugin_settings_service& settings;
};
