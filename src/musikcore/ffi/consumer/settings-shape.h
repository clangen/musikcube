#pragma once

#include <musikcore/settings.h>
#include "../common/primitives.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace musik { namespace core { namespace ffi {
    inline bool valid_setting_scope(mc_settings_scope scope) {
        return scope == MC_SETTINGS_APPLICATION || scope == MC_SETTINGS_SESSION || scope == MC_SETTINGS_HOTKEYS;
    }

    inline bool valid_setting_value(const mc_setting_value& value) {
        switch (value.type) {
            case MC_SETTING_BOOLEAN:
            case MC_SETTING_INTEGER:
                return true;
            case MC_SETTING_NUMBER:
                return std::isfinite(value.data.number);
            case MC_SETTING_TEXT:
                return valid_text(value.data.text);
        }
        return false;
    }

    inline bool valid_setting_entries(const mc_setting_entry* entries, size_t count) {
        constexpr auto maxEntries = std::numeric_limits<std::ptrdiff_t>::max() / sizeof(mc_setting_entry);
        if ((count && !entries) || count > maxEntries) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!valid_text(entries[i].key, false) || !valid_setting_value(entries[i].value)) {
                return false;
            }
        }
        return true;
    }
} } }
