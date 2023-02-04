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

#include "pch.hpp"

#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>

#include <filesystem>

#include "Locale.h"

#define KEY_STRINGS "strings"
#define KEY_DIMENSIONS "dimensions"
#define DEFAULT_LOCALE "en_US"

using namespace musik::core::i18n;
using namespace musik::core;
using namespace musik::core::prefs;

static nlohmann::json empty;

static nlohmann::json loadLocaleData(const std::string& fn) {
    char* bytes = nullptr;
    int count = 0;

    if (FileToByteArray(fn, &bytes, count, true) == true) {
        try {
            nlohmann::json localeData = nlohmann::json::parse(bytes);
            free(bytes);
            return localeData;
        }
        catch (...) {
        }

        free(bytes);
    }

    return nlohmann::json();
}

Locale::Locale() noexcept {
    this->prefs = Preferences::ForComponent(components::Settings);
    this->selectedLocale = prefs->GetString(keys::Locale, DEFAULT_LOCALE);
}

Locale::~Locale() {

}

void Locale::Initialize(const std::string& localePath) {
    namespace fs = std::filesystem;

    this->locales.clear();
    this->localePath = localePath;

    fs::path locales(fs::u8path(localePath));

    if (fs::exists(locales)) {
        fs::directory_iterator end;
        for (fs::directory_iterator it(locales); it != end; it++) {
            const fs::path& p = it->path();
            if (p.has_extension() && p.extension().u8string() == ".json") {
                std::string fn = p.filename().u8string();
                fn = fn.substr(0, fn.rfind("."));
                this->locales.push_back(fn);
            }
        }
    }

    this->SetSelectedLocale(this->selectedLocale);
}

std::vector<std::string> Locale::GetLocales() {
    std::vector<std::string> result;
    std::copy(this->locales.begin(), this->locales.end(), std::back_inserter(result));
    return result;
}

std::string Locale::GetSelectedLocale() {
    return this->selectedLocale;
}

bool Locale::SetSelectedLocale(const std::string& locale) {
    if (this->defaultLocaleData.is_null()) {
        this->defaultLocaleData = loadLocaleData(localePath + "/" + DEFAULT_LOCALE + ".json");
    }

    auto it = std::find_if(
        this->locales.begin(),
        this->locales.end(),
        [locale](std::string compare) noexcept {
            return locale == compare;
        });

    if (it != this->locales.end()) {
        std::string localeFn = this->localePath + "/" + locale + ".json";
        this->localeData = loadLocaleData(localeFn);

        if (!this->localeData.is_null()) {
            this->selectedLocale = locale;

            prefs->SetString(keys::Locale, this->selectedLocale.c_str());
            prefs->Save();

            this->LocaleChanged(this->selectedLocale);
            return true;
        }
    }

    return false;
}

std::string Locale::Translate(const std::string& key) {
    return this->Translate(key.c_str());
}

std::string Locale::Translate(const char* key) {
    /* get the string from the current locale */
    if (!this->localeData.is_null()) {
        const nlohmann::json& strings = this->localeData.value(KEY_STRINGS, empty);
        auto it = strings.find(key);
        if (it != strings.end()) {
            return it.value();
        }
    }

    /* can't be found? fall back to the default locale */
    if (!this->defaultLocaleData.is_null()) {
        const nlohmann::json& strings = this->defaultLocaleData.value(KEY_STRINGS, empty);
        auto it = strings.find(key);
        return (it != strings.end()) ? it.value() : key;
    }

    /* otherwise, just return the key! */
    return key;
}

int Locale::Dimension(const char* key, int defaultValue) {
    if (!this->localeData.is_null()) { /* current locale */
        const nlohmann::json& dimens = this->localeData.value(KEY_DIMENSIONS, empty);
        auto it = dimens.find(key);
        if (it != dimens.end()) {
            return it.value();
        }
    }

    if (!this->defaultLocaleData.is_null()) { /* fall back to default */
        const nlohmann::json& dimens = this->defaultLocaleData.value(KEY_DIMENSIONS, empty);
        return dimens.value(key, defaultValue);
    }

    return defaultValue; /* not found anywhere */
}
