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

#include "pch.hpp"
#include <core/support/Preferences.h>

#include <core/support/Common.h>
#include <core/db/CachedStatement.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

Preferences::Preferences(const char* nameSpace,const char* library) 
 :nameSpace(nameSpace)
 ,libraryId(0)
{
    this->IOPtr = IO::Instance();
    this->settings  = this->IOPtr->GetNamespace(nameSpace,library,this->libraryId);   
}

Preferences::~Preferences(void){
}


bool Preferences::GetBool(const char* key,bool defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);

    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(defaultValue);
    }

    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,defaultValue);
    return defaultValue;
}

int Preferences::GetInt(const char* key,int defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(defaultValue);
    }
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,defaultValue);
    return defaultValue;
}

std::string Preferences::GetString(const char* key,const char* defaultValue){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    IO::SettingMap::iterator setting = this->settings->find(key);
    if(setting!=this->settings->end()){
        return setting->second.Value(std::string(defaultValue));
    }
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,std::string(defaultValue));
    return defaultValue;
}

void Preferences::SetBool(const char* key,bool value){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,value);
}

void Preferences::SetInt(const char* key,int value){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,value);
}

void Preferences::SetString(const char* key,const char* value){
    boost::mutex::scoped_lock lock(IO::Instance()->mutex);
    this->IOPtr->SaveSetting(this->nameSpace.c_str(),this->libraryId,key,std::string(value));
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
    std::string dataDir   = GetDataDirectory();
    std::string dbFile    = GetDataDirectory() + "settings.db";
    this->db.Open(dbFile.c_str(),0,128);
    
    Preferences::CreateDB(this->db);

}

void Preferences::IO::SaveSetting(const char* nameSpace,int libraryId,const char *key,Setting setting){
    int nameSpaceId(0);
    db::CachedStatement getStmt("SELECT id FROM namespaces WHERE name=? AND library_id=?",this->db);
    getStmt.BindText(0,nameSpace);
    getStmt.BindInt(1,libraryId);

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
                insertSetting.BindText(3,setting.valueText);
                break;
        }
        insertSetting.Step();

        (*this->libraryNamespaces[libraryId][nameSpace])[key] = setting;
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

Preferences::Setting::Setting(std::string value) : type(3),valueText(value){
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
            this->valueText.assign(stmt.ColumnText(2));
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

std::string Preferences::Setting::Value(std::string defaultValue){
    switch(this->type){
        case Setting::Bool:
            return this->valueBool ? "1" : "0";
            break;
        case Setting::Int:
            try{
                return boost::lexical_cast<std::string>(this->valueInt);
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



Preferences::IO::SettingMapPtr Preferences::IO::GetNamespace(const char* nameSpace,const char* library,int &libraryId){

    boost::mutex::scoped_lock lock(this->mutex);

    if(library!=NULL){
        db::Statement getLibStmt("SELECT id FROM libraries WHERE name=?",this->db);
        getLibStmt.BindText(0,library);
        if(getLibStmt.Step()==db::Row){
            libraryId   = getLibStmt.ColumnInt(0);
        }
    }

    // First check if it's in the NamespaceMap
    NamespaceMap::iterator ns = this->libraryNamespaces[libraryId].find(nameSpace);
    if(ns!=this->libraryNamespaces[libraryId].end()){
        // Found namespace, return settings
        return ns->second;
    }


    // Not in cache, lets load it from db.
    int nameSpaceId(0);
    db::Statement getStmt("SELECT id FROM namespaces WHERE name=? AND library_id=?",this->db);
    getStmt.BindText(0,nameSpace);
    getStmt.BindInt(1,libraryId);

    SettingMapPtr newSettings( new SettingMap() );
    this->libraryNamespaces[libraryId][nameSpace] = newSettings;

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
        db::Statement insertNamespace("INSERT INTO namespaces (name,library_id) VALUES (?,?)",this->db);
        insertNamespace.BindText(0,nameSpace);
        insertNamespace.BindInt(1,libraryId);
        insertNamespace.Step();
        nameSpaceId = this->db.LastInsertedId();
        return newSettings;
    }
}

void Preferences::CreateDB(db::Connection &db){
    db.Execute("CREATE TABLE IF NOT EXISTS namespaces ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT)");

    // Add a library_id relation
    db.Execute("ALTER TABLE namespaces ADD COLUMN library_id INTEGER DEFAULT 0");

    db.Execute("CREATE TABLE IF NOT EXISTS settings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "namespace_id INTEGER DEFAULT 0,"
        "type INTEGER DEFAULT 0,"
        "the_key TEXT,"
        "the_value TEXT)");

    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS namespace_index ON namespaces (name,library_id)");
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS setting_index ON settings (namespace_id,the_key)");

	// Start by initializing the db
    db.Execute("CREATE TABLE IF NOT EXISTS libraries ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT,"
            "type INTEGER DEFAULT 0)");
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS library_index ON libraries (name)");

}

