#ifndef MUSIKCORE_PREFERENCES_HPP
#define MUSIKCORE_PREFERENCES_HPP

#include <musikcore/context.hpp>
#include <musikcore/preferences.h>

#include <initializer_list>

namespace musik { namespace core { namespace api {
    /* The entry borrows key and text storage through the enclosing write. */
    inline mc_preference_entry Preference(std::string_view key, bool value) {
        mc_preference_entry entry { { key.data(), key.size() }, { MC_PREFERENCE_BOOLEAN, {} } };
        entry.value.data.boolean = value;
        return entry;
    }

    inline mc_preference_entry Preference(std::string_view key, int32_t value) {
        mc_preference_entry entry { { key.data(), key.size() }, { MC_PREFERENCE_INTEGER, {} } };
        entry.value.data.integer = value;
        return entry;
    }

    inline mc_preference_entry Preference(std::string_view key, double value) {
        mc_preference_entry entry { { key.data(), key.size() }, { MC_PREFERENCE_NUMBER, {} } };
        entry.value.data.number = value;
        return entry;
    }

    inline mc_preference_entry Preference(std::string_view key, std::string_view value) {
        mc_preference_entry entry { { key.data(), key.size() }, { MC_PREFERENCE_TEXT, {} } };
        entry.value.data.text = { value.data(), value.size() };
        return entry;
    }

    inline mc_preference_entry Preference(std::string_view key, const char* value) {
        return Preference(key, value ? std::string_view(value) : std::string_view {});
    }

    /* Identifies a shared preference store. Copyable and movable. */
    class Preferences {
    public:
        constexpr explicit Preferences(mc_preference_scope scope) noexcept : scope(scope) {}

        bool GetBool(std::string_view key, bool fallback = false) const {
            mc_preference_value value { MC_PREFERENCE_BOOLEAN, {} };
            value.data.boolean = fallback;
            bool result = fallback;
            this->Read(key, value, [](void* data, const mc_preference_value* stored) {
                *static_cast<bool*>(data) = stored->data.boolean;
            }, &result);
            return result;
        }

        int32_t GetInt(std::string_view key, int32_t fallback = 0) const {
            mc_preference_value value { MC_PREFERENCE_INTEGER, {} };
            value.data.integer = fallback;
            int32_t result = fallback;
            this->Read(key, value, [](void* data, const mc_preference_value* stored) {
                *static_cast<int32_t*>(data) = stored->data.integer;
            }, &result);
            return result;
        }

        double GetDouble(std::string_view key, double fallback = 0.0) const {
            mc_preference_value value { MC_PREFERENCE_NUMBER, {} };
            value.data.number = fallback;
            double result = fallback;
            this->Read(key, value, [](void* data, const mc_preference_value* stored) {
                *static_cast<double*>(data) = stored->data.number;
            }, &result);
            return result;
        }

        std::string GetString(std::string_view key, std::string_view fallback = {}) const {
            mc_preference_value value { MC_PREFERENCE_TEXT, {} };
            value.data.text = { fallback.data(), fallback.size() };
            std::string result;
            this->Read(key, value, [](void* data, const mc_preference_value* stored) {
                *static_cast<std::string*>(data) = AsStringView(stored->data.text);
            }, &result);
            return result;
        }

        void SetBool(std::string_view key, bool value) {
            this->Write({ Preference(key, value) });
        }

        void SetInt(std::string_view key, int32_t value) {
            this->Write({ Preference(key, value) });
        }

        void SetDouble(std::string_view key, double value) {
            this->Write({ Preference(key, value) });
        }

        void SetString(std::string_view key, std::string_view value) {
            this->Write({ Preference(key, value) });
        }

        void Write(std::initializer_list<mc_preference_entry> entries) {
            const auto status = mc_preferences_write(this->scope, entries.begin(), entries.size());
            if (status != MC_OK) {
                throw Error(status);
            }
        }

        mc_result Save() {
            return mc_preferences_save(this->scope);
        }

    private:
        void Read(std::string_view key, const mc_preference_value& fallback, mc_preference_read_fn read, void* data) const {
            const auto status = mc_preferences_read(this->scope, { key.data(), key.size() }, &fallback, read, data);
            if (status != MC_OK) {
                throw Error(status);
            }
        }

        mc_preference_scope scope;
    };
} } }

#endif
