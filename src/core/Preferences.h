//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 team
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

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

class Preferences{
    public:
        Preferences(const char* nameSpace);
        ~Preferences(void);

        bool GetBool(const char* key,bool defaultValue);
        int GetInt(const char* key,int defaultValue);
        const char* GetString(const char* key,const char* defaultValue);
        const wchar_t* GetString(const char* key,const wchar_t* defaultValue);

        void SetBool(const char* key,bool value);
        void SetInt(const char* key,int value);
        void SetString(const char* key,const char* value);
        void SetString(const char* key,const wchar_t* value);
        void SetString(const char* key,const std::string &value);
        void SetString(const char* key,const std::wstring &value);

    private:
        int nameSpaceId;

        void SaveSetting(const char* key,std::string &value);

        std::string nameSpace;
        typedef std::map<std::string,std::string> SettingsMap;
        SettingsMap cachedSettings;
        void GetSettings();

        class IO{
            public:
                IO(void);
                ~IO(void);

                db::Connection db;

                typedef boost::shared_ptr<IO> Ptr;
                static IO::Ptr Instance();
            private:
                static IO::Ptr sInstancePtr;
        };

        IO::Ptr IOPtr;

};

//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////
