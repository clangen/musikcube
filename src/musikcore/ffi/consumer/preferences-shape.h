#pragma once

#include <musikcore/preferences.h>
#include "../common/primitives.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace musik { namespace core { namespace ffi {
    inline bool valid_preference_scope(mc_preference_scope scope) {
        return scope == MC_PREFERENCE_SETTINGS || scope == MC_PREFERENCE_SESSION || scope == MC_PREFERENCE_HOTKEYS;
    }

    inline bool valid_preference_value(const mc_preference_value& value) {
        switch (value.type) {
            case MC_PREFERENCE_BOOLEAN:
            case MC_PREFERENCE_INTEGER:
                return true;
            case MC_PREFERENCE_NUMBER:
                return std::isfinite(value.data.number);
            case MC_PREFERENCE_TEXT:
                return valid_text(value.data.text);
        }
        return false;
    }

    inline bool valid_preference_entries(const mc_preference_entry* entries, size_t count) {
        constexpr auto maxEntries = std::numeric_limits<std::ptrdiff_t>::max() / sizeof(mc_preference_entry);
        if ((count && !entries) || count > maxEntries) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!valid_text(entries[i].key, false) || !valid_preference_value(entries[i].value)) {
                return false;
            }
        }
        return true;
    }
} } }
