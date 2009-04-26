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
#include <boost/thread/mutex.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

class Preferences{
    public:
        Preferences(const char* nameSpace,const utfchar* library=NULL);
        ~Preferences(void);

        bool GetBool(const char* key,bool defaultValue);
        int GetInt(const char* key,int defaultValue);
        utfstring GetString(const char* key,const utfchar* defaultValue);

        void SetBool(const char* key,bool value);
        void SetInt(const char* key,int value);
        void SetString(const char* key,const utfchar* value);

        std::string nameSpace;
        int libraryId;

        static void CreateDB(db::Connection &db);

    private:

        class Setting{

            public:
                Setting();
                Setting(bool value);
                Setting(int value);
                Setting(utfstring value);
                Setting(db::Statement &stmt);

                typedef enum {
                    Bool=1,
                    Int=2,
                    Text=3
                } Type;

                int type;
                int valueInt;
                bool valueBool;
                utfstring valueText;

                bool Value(bool defaultValue);
                int Value(int defaultValue);
                utfstring Value(utfstring defaultValue);
        };
    private:


        class IO{
            public:
                IO(void);
                ~IO(void);

                typedef std::map<std::string,Setting> SettingMap;
                typedef boost::shared_ptr<SettingMap> SettingMapPtr;
                typedef std::map<std::string,SettingMapPtr> NamespaceMap;
                typedef std::map<int,NamespaceMap> LibNamespaceMap;
                typedef boost::shared_ptr<IO> Ptr;

                SettingMapPtr GetNamespace(const char* nameSpace,const utfchar* library,int &libraryId);

                void SaveSetting(const char* nameSpace,int libraryId,const char *key,Setting setting);

                static IO::Ptr Instance();

                boost::mutex mutex;

            private:
                db::Connection db;
                LibNamespaceMap libraryNamespaces;
                

        };


        IO::Ptr IOPtr;
        IO::SettingMapPtr settings;

};

//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////
