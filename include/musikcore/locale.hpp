#ifndef MUSIKCORE_LOCALE_HPP
#define MUSIKCORE_LOCALE_HPP

#include <musikcore/locale.h>

#include <string>
#include <string_view>
#include <vector>

namespace musik { namespace core { namespace api {

    inline mc_result LoadLocales(std::string_view directory) {
        return mc_locale_load({ directory.data(), directory.size() });
    }

    inline std::vector<std::string> ReadLocales() {
        std::vector<std::string> locales;
        mc_locale_list([](void* data, mc_string_view text) {
            static_cast<std::vector<std::string>*>(data)->emplace_back(text.data, text.length);
        }, &locales);
        return locales;
    }

    inline std::string ReadSelectedLocale() {
        std::string locale;
        mc_locale_read_selected([](void* data, mc_string_view text) {
            static_cast<std::string*>(data)->assign(text.data, text.length);
        }, &locale);
        return locale;
    }

    inline mc_result SelectLocale(std::string_view locale) {
        return mc_locale_select({ locale.data(), locale.size() });
    }

    inline std::string Translate(std::string_view key) {
        std::string text;
        if (mc_locale_translate({ key.data(), key.size() }, [](void* data, mc_string_view translation) {
            static_cast<std::string*>(data)->assign(translation.data, translation.length);
        }, &text) != MC_OK)
        {
            return std::string(key);
        }
        return text;
    }

    inline int32_t LocaleInteger(std::string_view key, int32_t fallback) {
        int32_t value = fallback;
        mc_locale_read_integer({ key.data(), key.size() }, &value);
        return value;
    }

} } }

#endif
