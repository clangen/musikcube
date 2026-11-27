#ifndef MUSIKCORE_SETTINGS_HPP
#define MUSIKCORE_SETTINGS_HPP

#include <musikcore/context.hpp>
#include <musikcore/settings.h>

#include <initializer_list>
#include <variant>
#include <vector>

namespace musik { namespace core { namespace api {
    using SettingValue = std::variant<bool, int32_t, double, std::string>;

    /* Owns an editable setting and its constraints. Copyable and movable. */
    struct SettingField {
        std::string key;
        SettingValue value;
        double minimum = 0;
        double maximum = 0;
        int32_t precision = 0;
        std::vector<std::string> choices;
        bool allowEmpty = false;
    };

    /* The entry borrows key and text storage through the enclosing write. */
    inline mc_setting_entry Setting(std::string_view key, bool value) {
        mc_setting_entry entry { { key.data(), key.size() }, { MC_SETTING_BOOLEAN, {} } };
        entry.value.data.boolean = value;
        return entry;
    }

    inline mc_setting_entry Setting(std::string_view key, int32_t value) {
        mc_setting_entry entry { { key.data(), key.size() }, { MC_SETTING_INTEGER, {} } };
        entry.value.data.integer = value;
        return entry;
    }

    inline mc_setting_entry Setting(std::string_view key, double value) {
        mc_setting_entry entry { { key.data(), key.size() }, { MC_SETTING_NUMBER, {} } };
        entry.value.data.number = value;
        return entry;
    }

    inline mc_setting_entry Setting(std::string_view key, std::string_view value) {
        mc_setting_entry entry { { key.data(), key.size() }, { MC_SETTING_TEXT, {} } };
        entry.value.data.text = { value.data(), value.size() };
        return entry;
    }

    inline mc_setting_entry Setting(std::string_view key, const char* value) {
        return Setting(key, value ? std::string_view(value) : std::string_view {});
    }

    /* Identifies a shared setting store. Copyable and movable. */
    class Settings {
    public:
        constexpr explicit Settings(mc_settings_scope scope) noexcept : scope(scope) {}

        bool GetBool(std::string_view key, bool fallback = false) const {
            mc_setting_value value { MC_SETTING_BOOLEAN, {} };
            value.data.boolean = fallback;
            bool result = fallback;
            this->Read(key, value, [](void* data, const mc_setting_value* stored) {
                *static_cast<bool*>(data) = stored->data.boolean;
            }, &result);
            return result;
        }

        int32_t GetInt(std::string_view key, int32_t fallback = 0) const {
            mc_setting_value value { MC_SETTING_INTEGER, {} };
            value.data.integer = fallback;
            int32_t result = fallback;
            this->Read(key, value, [](void* data, const mc_setting_value* stored) {
                *static_cast<int32_t*>(data) = stored->data.integer;
            }, &result);
            return result;
        }

        double GetDouble(std::string_view key, double fallback = 0.0) const {
            mc_setting_value value { MC_SETTING_NUMBER, {} };
            value.data.number = fallback;
            double result = fallback;
            this->Read(key, value, [](void* data, const mc_setting_value* stored) {
                *static_cast<double*>(data) = stored->data.number;
            }, &result);
            return result;
        }

        std::string GetString(std::string_view key, std::string_view fallback = {}) const {
            mc_setting_value value { MC_SETTING_TEXT, {} };
            value.data.text = { fallback.data(), fallback.size() };
            std::string result;
            this->Read(key, value, [](void* data, const mc_setting_value* stored) {
                *static_cast<std::string*>(data) = AsStringView(stored->data.text);
            }, &result);
            return result;
        }

        void SetBool(std::string_view key, bool value) {
            this->Write({ Setting(key, value) });
        }

        void SetInt(std::string_view key, int32_t value) {
            this->Write({ Setting(key, value) });
        }

        void SetDouble(std::string_view key, double value) {
            this->Write({ Setting(key, value) });
        }

        void SetString(std::string_view key, std::string_view value) {
            this->Write({ Setting(key, value) });
        }

        void Write(std::initializer_list<mc_setting_entry> entries) {
            this->Write(entries.begin(), entries.size());
        }

        void Write(const std::vector<mc_setting_entry>& entries) {
            this->Write(entries.data(), entries.size());
        }

        std::vector<std::string> Keys() const {
            std::vector<std::string> keys;
            const auto status = mc_settings_list_keys(this->scope, [](void* data, mc_string_view key) {
                static_cast<std::vector<std::string>*>(data)->emplace_back(AsStringView(key));
            }, &keys);
            if (status != MC_OK) {
                throw Error(status);
            }
            return keys;
        }

        mc_result Save() {
            return mc_settings_save(this->scope);
        }

    private:
        void Write(const mc_setting_entry* entries, size_t count) {
            const auto status = mc_settings_write(this->scope, entries, count);
            if (status != MC_OK) {
                throw Error(status);
            }
        }

        void Read(std::string_view key, const mc_setting_value& fallback, mc_setting_read_fn read, void* data) const {
            const auto status = mc_settings_read(this->scope, { key.data(), key.size() }, &fallback, read, data);
            if (status != MC_OK) {
                throw Error(status);
            }
        }

        mc_settings_scope scope;
    };
} } }

#endif
