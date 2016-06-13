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
#include <core/library/LibraryFactory.h>
#include <core/library/LocalLibrary.h>
#include <core/db/Connection.h>
#include <core/support/Common.h>
#include <core/support/Preferences.h>

using namespace musik::core;

LibraryFactory& LibraryFactory::Instance() { 
    typedef std::shared_ptr<LibraryFactory> InstanceType;
    static InstanceType sInstance(new LibraryFactory());
    return *sInstance; 
};


//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
LibraryFactory::LibraryFactory() {
    // Connect to the settings.db
    std::string dataDir = GetDataDirectory();
    std::string dbFile = GetDataDirectory() + "settings.db";
    musik::core::db::Connection db;
    db.Open(dbFile.c_str(), 0, 128);

    Preferences::CreateDB(db);

    // Get the libraries
    db::Statement stmtGetLibs("SELECT id, name, type FROM libraries ORDER BY id", db);

    while(stmtGetLibs.Step() == db::Row) {
        int id = stmtGetLibs.ColumnInt(0);
        std::string name = stmtGetLibs.ColumnText(1);
        int type = stmtGetLibs.ColumnInt(2);
        this->AddLibrary(id, type, name);
    }

    // If there are no libraries, add a LocalLibrary
    if (this->libraries.empty()) {
        this->CreateLibrary("Local Library", LocalLibrary);
    }

}

LibraryFactory::~LibraryFactory() {
}

//////////////////////////////////////////
///\brief
///Add a new library to the LibraryFactory
///
///\param name
///Identifier of library. Need to be a unique name.
///
///\param type
///Type of library. See LibraryFactory::Types
///
///\param sendEvent
///Send the LibrariesUpdated when library has been added?
///
///\param startup
///Start the library when added
///
///\returns
///LibraryPtr of the added library. (NULL pointer on failure)
//////////////////////////////////////////
LibraryPtr LibraryFactory::AddLibrary(int id, int type, const std::string& name)
{
    LibraryPtr library = library::LocalLibrary::Create(name, id);
    
    if (library) {
        this->libraries.push_back(library);
        this->libraryMap[id] = library;
        this->LibrariesUpdated();
    }

    return library;
}

void LibraryFactory::Shutdown() {
    Instance().libraries.clear();
}

//////////////////////////////////////////
///\brief
///Create a new Library
///
///\param name
///Identifier of library. Need to be a unique name.
///
///\param type
///Type of library. See LibraryFactory::Types
///
///\param startup
///Start the library when added
///
///\returns
///LibraryPtr of the added library. (NULL pointer on failure)
//////////////////////////////////////////
LibraryPtr LibraryFactory::CreateLibrary(const std::string& name, int type) {
    // Connect to the settings.db
    std::string dataDir = GetDataDirectory();
    std::string dbFile = GetDataDirectory() + "settings.db";
    musik::core::db::Connection db;
    db.Open(dbFile.c_str(), 0, 128);

    db::Statement stmtInsert("INSERT OR FAIL INTO libraries (name,type) VALUES (?,?)", db);
    stmtInsert.BindText(0, name);
    stmtInsert.BindInt(1, type);

    if (stmtInsert.Step() == db::Done) {
        return this->AddLibrary(db.LastInsertedId(), type, name);
    }

    return LibraryPtr();
}

//////////////////////////////////////////
///\brief
///Get the vector with all current libraries
//////////////////////////////////////////
LibraryFactory::LibraryVector& LibraryFactory::Libraries(){
    return LibraryFactory::Instance().libraries;
}

LibraryPtr LibraryFactory::GetLibrary(int identifier){
    if (identifier) {
        LibraryMap::iterator lib = this->libraryMap.find(identifier);
        if (lib != this->libraryMap.end()) {
            return lib->second;
        }
    }
    return LibraryPtr();
}

