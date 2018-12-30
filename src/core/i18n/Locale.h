//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/config.h>
#include <core/support/Preferences.h>
#include <unordered_map>
#include <sigslot/sigslot.h>
#include <json.hpp>

namespace musik { namespace core { namespace i18n {

    class Locale {
        public:
            sigslot::signal1<std::string> LocaleChanged;

            ~Locale();

            static Locale& Instance() {
                static Locale instance;
                return instance;
            }

            void Initialize(const std::string& localePath);

            std::vector<std::string> GetLocales();
            std::string GetSelectedLocale();
            bool SetSelectedLocale(const std::string& locale);

            std::string Translate(const std::string& key);
            std::string Translate(const char* key);

            int Dimension(const char* key, int defaultValue);

        private:
            Locale();

            std::vector<std::string> locales;
            std::shared_ptr<musik::core::Preferences> prefs;
            std::string selectedLocale;
            std::string localePath;
            nlohmann::json localeData;
            nlohmann::json defaultLocaleData;
    };

    #define _TSTR(KEY) (musik::core::i18n::Locale::Instance().Translate(KEY))
    #define _TCP(KEY) (musik::core::i18n::Locale::Instance().Translate(KEY).c_str())
    #define _DIMEN(KEY, DEFAULT) (musik::core::i18n::Locale::Instance().Dimension(KEY, DEFAULT))

} } }

