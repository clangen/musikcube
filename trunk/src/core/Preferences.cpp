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

#include "pch.hpp"
#include <core/Preferences.h>

#include <core/Common.h>
#include <core/db/CachedStatement.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

Preferences::Preferences(const char* nameSpace) 
 :nameSpace(nameSpace)
{
    this->IOPtr = IO::Instance();
    this->settings  = this->IOPtr->GetNamespace(nameSpace);   
}

Preferences::~Preferences(void){
}


bool Preferences::GetBool(const char* key,bool defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);

    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(defaultValue);
    }
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),key,Setting(defaultValue));
    return defaultValue;
}

int Preferences::GetInt(const char* key,int defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(defaultValue);
    }
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),key,Setting(defaultValue));
    return defaultValue;
}

utfstring Preferences::GetString(const char* key,const utfchar* defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(utfstring(defaultValue));
    }
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),key,Setting(utfstring(defaultValue)));
    return defaultValue;
}

//////////////////////////////////////////////////////////////////////////////

Preferences::IO::Ptr Preferences::IO::Instance(){
    static boost::mutex instanceMutex;
    boost::mutex::scoped_lock oLock(instanceMutex);

    static IO::Ptr sInstance(new Preferences::IO());
    return sInstance;
}

Preferences::IO::IO(void){
    boost::mutex::scoped_lock lock(this->mutex);
    utfstring dataDir   = GetDataDirectory();
    utfstring dbFile    = GetDataDirectory() + UTF("settings.db");
    this->db.Open(dbFile.c_str(),0,128);

    this->db.Execute("CREATE TABLE IF NOT EXISTS namespaces ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS settings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "namespace_id INTEGER DEFAULT 0,"
        "type INTEGER DEFAULT 0,"
        "the_key TEXT,"
        "the_value TEXT)");

    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS namespace_index ON namespaces (name)");
    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS setting_index ON settings (namespace_id,the_key)");

}

void Preferences::IO::SaveSetting(const char* nameSpace,const char *key,Setting &setting){
    int nameSpaceId(0);
    db::CachedStatement getStmt("SELECT id FROM namespaces WHERE name=?",this->db);
    getStmt.BindText(0,nameSpace);

    if(getStmt.Step()==db::Row){
        nameSpaceId = getStmt.ColumnInt(0);
        db::Statement insertSetting("INSERT OR REPLACE INTO settings (namespace_id,type,the_key,the_value) VALUES (?,?,?,?)",this->db);
        insertSetting.BindInt(0,nameSpaceId);
        insertSetting.BindInt(1,setting.type);
        insertSetting.BindText(2,key);
        switch(setting.type){
            case (Setting::Bool):
                insertSetting.BindInt(3,setting.valueBool?1:0);
                break;
            case (Setting::Int):
                insertSetting.BindInt(3,setting.valueInt);
                break;
            case (Setting::Text):
                insertSetting.BindTextUTF(3,setting.valueText);
                break;
        }
        insertSetting.Step();

        (*this->namespaces[nameSpace])[key] = setting;
    }
}

Preferences::IO::~IO(void){
    this->db.Close();
}

Preferences::Setting::Setting() : type(0){
}

Preferences::Setting::Setting(bool value) : type(1),valueBool(value){
}

Preferences::Setting::Setting(int value) : type(2),valueInt(value){
}

Preferences::Setting::Setting(utfstring value) : type(3),valueText(value){
}


Preferences::Setting::Setting(db::Statement &stmt) : 
    type(stmt.ColumnInt(0)) {
    switch(type){
        case Setting::Int:
            this->valueInt  = stmt.ColumnInt(2);
            break;
        case Setting::Bool:
            this->valueBool = (stmt.ColumnInt(2)>0);
            break;
        default:
            this->valueText.assign(stmt.ColumnTextUTF(2));
    }
}

bool Preferences::Setting::Value(bool defaultValue){
    switch(this->type){
        case Setting::Bool:
            return this->valueBool;
            break;
        case Setting::Int:
            return this->valueInt>0;
            break;
        case Setting::Text:
            return !this->valueText.empty();
            break;
    }
    return defaultValue;
}

int Preferences::Setting::Value(int defaultValue){
    switch(this->type){
        case Setting::Bool:
            return this->valueBool?1:0;
            break;
        case Setting::Int:
            return this->valueInt;
            break;
        case Setting::Text:
            try{
                return boost::lexical_cast<int>(this->valueText);
            }
            catch(...){
            }
            break;
    }
    return defaultValue;
}

utfstring Preferences::Setting::Value(utfstring defaultValue){
    switch(this->type){
        case Setting::Bool:
            return this->valueBool?UTF("1"):UTF("0");
            break;
        case Setting::Int:
            try{
                return boost::lexical_cast<utfstring>(this->valueInt);
            }
            catch(...){
            }
            break;
        case Setting::Text:
            return this->valueText;
            break;
    }
    return defaultValue;
}



Preferences::IO::SettingMapPtr Preferences::IO::GetNamespace(const char* nameSpace){

    boost::mutex::scoped_lock lock(this->mutex);
    // First check if it's in the NamespaceMap
    NamespaceMap::iterator ns = this->namespaces.find(nameSpace);
    if(ns!=this->namespaces.end()){
        // Found namespace, return settings
        return ns->second;
    }

    // Not in cache, lets load it from db.
    int nameSpaceId(0);
    db::CachedStatement getStmt("SELECT id FROM namespaces WHERE name=?",this->db);
    getStmt.BindText(0,nameSpace);

    SettingMapPtr newSettings( new SettingMap() );
    this->namespaces[nameSpace] = newSettings;

    if(getStmt.Step()==db::Row){
        // Namespace exists, load the settings
        nameSpaceId = getStmt.ColumnInt(0);

        db::Statement selectSettings("SELECT type,the_key,the_value FROM settings WHERE namespace_id=?",this->db);
        selectSettings.BindInt(0,nameSpaceId);
        while( selectSettings.Step()==db::Row ){
            (*newSettings)[selectSettings.ColumnText(1)]  = Setting(selectSettings);
        }

        return newSettings;
    }else{
        // First time namespace is accessed, create it.
        db::Statement insertNamespace("INSERT INTO namespaces (name) VALUES (?)",this->db);
        insertNamespace.BindText(0,nameSpace);
        insertNamespace.Step();
        nameSpaceId = this->db.LastInsertedId();
        return newSettings;
    }
}

