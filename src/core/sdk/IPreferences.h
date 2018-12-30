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

#include <stddef.h>

namespace musik { namespace core { namespace sdk {

    class IPreferences {
        public:
            virtual void Release() = 0;

            virtual bool GetBool(const char* key, bool defaultValue = false) = 0;
            virtual int GetInt(const char* key, int defaultValue = 0) = 0;
            virtual double GetDouble(const char* key, double defaultValue = 0.0f) = 0;
            virtual int GetString(const char* key, char* dst, size_t size, const char* defaultValue = "") = 0;

            virtual void SetBool(const char* key, bool value) = 0;
            virtual void SetInt(const char* key, int value) = 0;
            virtual void SetDouble(const char* key, double value) = 0;
            virtual void SetString(const char* key, const char* value) = 0;

            virtual void Save() = 0;
    };

    template <typename String>
    String getPreferenceString(IPreferences* prefs, const char* key, const char* defaultValue) {
        if (prefs) {
            size_t count = prefs->GetString(key, nullptr, 0, defaultValue);
            if (count) {
                char* buffer = new char[count];
                prefs->GetString(key, buffer, count, defaultValue);
                String result = buffer;
                delete[] buffer;
                return result;
            }
        }
        return "";
    }

} } }

