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
#include "musikcore/sdk/Filesystem.h"

#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>

#include <filesystem>

#include <musikcore/locale.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/common/primitives.h>

#include "Locale.h"

#define KEY_STRINGS "strings"
#define DEFAULT_LOCALE "en_US"

using musik::core::sdk::fs::pathToUtf8;

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

    std::lock_guard lock(this->mutex);
    this->locales.clear();
    this->defaultLocaleData = {};
    this->localeData = {};
    this->localePath = localePath;

    fs::path locales(musik::core::sdk::fs::pathFromUtf8(localePath));

    if (fs::exists(locales)) {
        fs::directory_iterator end;
        for (fs::directory_iterator it(locales); it != end; it++) {
            const fs::path& p = it->path();
            if (p.has_extension() && pathToUtf8(p.extension()) == ".json") {
                std::string fn = pathToUtf8(p.filename());
                fn = fn.substr(0, fn.rfind("."));
                this->locales.push_back(fn);
            }
        }
    }

    this->LoadSelection(this->selectedLocale);
}

std::vector<std::string> Locale::GetLocales() {
    std::lock_guard lock(this->mutex);
    return this->locales;
}

std::string Locale::GetSelectedLocale() {
    std::lock_guard lock(this->mutex);
    return this->selectedLocale;
}

bool Locale::SetSelectedLocale(const std::string& locale) {
    std::lock_guard lock(this->mutex);
    return this->LoadSelection(locale);
}

bool Locale::LoadSelection(const std::string& locale) {
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
        auto selected = loadLocaleData(localeFn);

        if (!selected.is_null()) {
            this->localeData = std::move(selected);
            this->selectedLocale = locale;

            prefs->SetString(keys::Locale, this->selectedLocale.c_str());
            prefs->Save();

            return true;
        }
    }

    return false;
}

std::string Locale::Translate(const std::string& key) {
    return this->Translate(key.c_str());
}

std::string Locale::Translate(const char* key) {
    std::lock_guard lock(this->mutex);

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

bool Locale::ReadInteger(const std::string& key, int32_t& value) {
    std::lock_guard lock(this->mutex);

    for (const auto* resource : { &this->localeData, &this->defaultLocaleData }) {
        const auto integers = resource->find("integers");
        if (integers == resource->end() || !integers->is_object()) {
            continue;
        }

        const auto entry = integers->find(key);
        if (entry == integers->end() || !entry->is_number_integer()) {
            continue;
        }

        if (entry->is_number_unsigned()) {
            if (entry->get<uint64_t>() > INT32_MAX) {
                continue;
            }
        }
        else {
            const auto number = entry->get<int64_t>();
            if (number < INT32_MIN || number > INT32_MAX) {
                continue;
            }
        }

        value = entry->get<int32_t>();
        return true;
    }

    return false;
}

namespace {
    void readLocaleText(mc_locale_text_fn read, void* userData, const std::string& text) {
        read(userData, { text.data(), text.size() });
    }
}

mc_result MC_CALL mc_locale_load(mc_string_view directory) {
    if (!ffi::valid_text(directory, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        Locale::Instance().Initialize(std::string(directory.data, directory.length));
        return MC_OK;
    });
}

mc_result MC_CALL mc_locale_list(mc_locale_text_fn read, void* userData) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        for (const auto& locale : Locale::Instance().GetLocales()) {
            readLocaleText(read, userData, locale);
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_locale_read_selected(mc_locale_text_fn read, void* userData) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        readLocaleText(read, userData, Locale::Instance().GetSelectedLocale());
        return MC_OK;
    });
}

mc_result MC_CALL mc_locale_select(mc_string_view locale) {
    if (!ffi::valid_text(locale, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        return Locale::Instance().SetSelectedLocale(std::string(locale.data, locale.length))
            ? MC_OK
            : MC_ERROR_NOT_FOUND;
    });
}

mc_result MC_CALL mc_locale_translate(mc_string_view key, mc_locale_text_fn read, void* userData) {
    if (!read || !ffi::valid_text(key, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        readLocaleText(read, userData, Locale::Instance().Translate(std::string(key.data, key.length)));
        return MC_OK;
    });
}

mc_result MC_CALL mc_locale_read_integer(mc_string_view key, int32_t* value) {
    if (!value || !ffi::valid_text(key, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return ffi::exception_barrier(nullptr, [&] {
        return Locale::Instance().ReadInteger(std::string(key.data, key.length), *value)
            ? MC_OK : MC_ERROR_NOT_FOUND;
    });
}
