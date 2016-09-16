//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <boost/shared_ptr.hpp>
#include <vector>

#include <core/config.h>
#include <core/db/Connection.h>
#include <boost/thread/mutex.hpp>

#include <json.hpp>

namespace musik { namespace core {
    class Preferences {
        public:
            enum Mode {
                ModeReadOnly,
                ModeReadWrite,
                ModeAutoSave
            };

            static std::shared_ptr<Preferences>
                ForComponent(const std::string& c, Mode mode = ModeAutoSave);

            ~Preferences();

            bool GetBool(const std::string& key, bool defaultValue = false);
            int GetInt(const std::string& key, int defaultValue = 0);
            std::string GetString(const std::string& key, const std::string& defaultValue = "");

            void SetBool(const std::string& key, bool value);
            void SetInt(const std::string& key, int value);
            void SetString(const std::string& key, const char* value);

            void GetKeys(std::vector<std::string>& target);
            void Save();

        private:
            Preferences(const std::string& component, Mode mode);
            void Load();

            boost::mutex mutex;
            nlohmann::json json;
            std::string component;
            Mode mode;
    };

} }

