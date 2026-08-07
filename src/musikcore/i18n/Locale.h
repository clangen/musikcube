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

#pragma once

/** @file Locale.h
 *  @brief Internationalization support: loading and translating locale strings.
 *  @details Locale is a singleton that loads JSON locale files from a directory,
 *      resolves human-readable strings by key, and exposes translation helpers.
 *      Locale data falls back to the default locale when a key is missing. */

#include <musikcore/config.h>
#include <musikcore/support/Preferences.h>
#include <unordered_map>

#pragma warning(push, 0)
#include <sigslot/sigslot.h>
#include <nlohmann/json.hpp>
#pragma warning(pop)

/** @namespace musik::core::i18n
 *  @brief Internationalization and locale-aware string translation. */
namespace musik { namespace core { namespace i18n {

    /** @brief Loads and translates application strings from JSON locale files.
     *  @details Accessible via Instance(). Initialize() must be called once with
     *      the directory containing the locale files. The currently selected
     *      locale is persisted in preferences. */
    class Locale {
        public:
            /** @brief Emitted when the selected locale changes. */
            sigslot::signal1<std::string> LocaleChanged;

            ~Locale();

            /** @return The process-wide Locale singleton. */
            static Locale& Instance() {
                static Locale instance;
                return instance;
            }

            /** @brief Loads the available locales from the given directory.
             *  @param localePath Directory containing JSON locale files. */
            void Initialize(const std::string& localePath);

            /** @return The names of all available locales. */
            std::vector<std::string> GetLocales();
            /** @return The name of the currently selected locale. */
            std::string GetSelectedLocale();
            /** @brief Selects and activates a locale.
             *  @param locale The locale name to select.
             *  @return true if the locale was selected successfully. */
            bool SetSelectedLocale(const std::string& locale);

            /** @brief Translates the given key into the selected locale.
             *  @param key The translation key.
             *  @return The translated string, or the key itself if not found. */
            std::string Translate(const std::string& key);
            /** @brief Translates the given key into the selected locale.
             *  @param key The translation key (C string).
             *  @return The translated string, or the key itself if not found. */
            std::string Translate(const char* key);

            /** @brief Reads a dimension value from the locale data.
             *  @param key The dimension key.
             *  @param defaultValue Value returned when the key is missing.
             *  @return The locale dimension value, or defaultValue. */
            int Dimension(const char* key, int defaultValue);

        private:
            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(Locale)

            /** @brief Creates the singleton (private). */
            Locale() noexcept;

            std::vector<std::string> locales; /**< Names of available locales. */
            std::shared_ptr<musik::core::Preferences> prefs; /**< Preferences holding the selection. */
            std::string selectedLocale; /**< Currently active locale name. */
            std::string localePath;     /**< Directory holding locale files. */
            nlohmann::json localeData;  /**< Loaded strings for the selected locale. */
            nlohmann::json defaultLocaleData; /**< Fallback strings from the default locale. */
    };

    #define _TSTR(KEY) (musik::core::i18n::Locale::Instance().Translate(KEY))
    #define _TCP(KEY) (musik::core::i18n::Locale::Instance().Translate(KEY).c_str())
    #define _DIMEN(KEY, DEFAULT) (musik::core::i18n::Locale::Instance().Dimension(KEY, DEFAULT))

} } }

