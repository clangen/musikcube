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
#include "Preferences.h"

#include <core/Common.h>
#include <core/db/CachedStatement.h>

#include <boost/lexical_cast.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

Preferences::Preferences(const char* nameSpace) : 
    nameSpace(nameSpace),
    nameSpaceId(0),
    IOPtr(IO::Instance())
{
    this->GetSettings();   
}

Preferences::~Preferences(void){
}

void Preferences::SaveSetting(const char* key,std::string &value){
    db::CachedStatement saveStmt("INSERT INTO settings (namespace_id,the_key,the_value) VALUES (?,?,?)",this->IOPtr->db);
    saveStmt.BindInt(0,this->nameSpaceId);
    saveStmt.BindText(1,key);
    saveStmt.BindText(2,value.c_str());
    saveStmt.Step();

    this->cachedSettings[key]   = value;
}

bool Preferences::GetBool(const char* key,bool defaultValue){
    SettingsMap::iterator setting = this->cachedSettings.find(key);
    if(setting!=this->cachedSettings.end()){
        try{
            return boost::lexical_cast<bool>(setting->second);
        }
        catch(...){
            return defaultValue;
        }
    }
    std::string value(defaultValue?"1":"0");
    this->SaveSetting(key,value);
    return defaultValue;
}
/*
int Preferences::GetInt(const char* key,int defaultValue){
}

const char* Preferences::GetString(const char* key,const char* defaultValue){
}

const wchar_t* Preferences::GetString(const char* key,const wchar_t* defaultValue){
}

void Preferences::SetBool(const char* key,bool value){
}

void Preferences::SetInt(const char* key,int value){
}

void Preferences::SetString(const char* key,const char* value){
}

void Preferences::SetString(const char* key,const wchar_t* value){
}

void Preferences::SetString(const char* key,const std::string &value){
}

void Preferences::SetString(const char* key,const std::wstring &value){
}
*/


void Preferences::GetSettings(){

    {
        db::CachedStatement getStmt("SELECT id FROM namespaces WHERE name=?",this->IOPtr->db);
        getStmt.BindText(0,this->nameSpace.c_str());

        if(getStmt.Step()==db::Row){
            this->nameSpaceId = getStmt.ColumnInt(0);
        }else{
            db::Statement insertNamespace("INSERT INTO namespaces (name) VALUES (?)",this->IOPtr->db);
            insertNamespace.Step();
            this->nameSpaceId = this->IOPtr->db.LastInsertedId();
        }
    }
    
    // Get the settings
    db::CachedStatement getSettings("SELECT the_key,the_value FROM settings WHERE namespace_id=?",this->IOPtr->db);
    getSettings.BindInt(0,this->nameSpaceId);

    while(getSettings.Step()==db::Row){
        this->cachedSettings[getSettings.ColumnText(0)] = getSettings.ColumnText(1);
    }

}



//////////////////////////////////////////////////////////////////////////////

Preferences::IO::Ptr Preferences::IO::sInstancePtr;

Preferences::IO::Ptr Preferences::IO::Instance(){
    if(!Preferences::IO::sInstancePtr){
        Preferences::IO::sInstancePtr.reset(new Preferences::IO());
    }
    return Preferences::IO::sInstancePtr;
}

Preferences::IO::IO(void){
    utfstring dataDir   = GetDataDirectory();
    utfstring dbFile    = GetDataDirectory() + UTF("settings.db");
    this->db.Open(dbFile.c_str(),0,128);

    this->db.Execute("CREATE TABLE IF NOT EXISTS namespaces ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS settings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "namespace_id INTEGER DEFAULT 0,"
        "the_key TEXT,"
        "the_value TEXT)");

    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS namespace_index ON namespaces (name)");
    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS setting_index ON settings (namespace_id,the_key)");

}

Preferences::IO::~IO(void){
    this->db.Close();
}

