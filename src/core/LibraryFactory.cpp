//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include <core/LibraryFactory.h>
#include <core/Library/LocalDB.h>
#include <core/Library/Remote.h>
#include <core/db/Connection.h>
#include <core/Common.h>
#include <core/Preferences.h>

using namespace musik::core;

LibraryFactory LibraryFactory::sInstance;

LibraryFactory::LibraryFactory(void){
	// Connect to the settings.db
	utfstring dataDir   = GetDataDirectory();
    utfstring dbFile    = GetDataDirectory() + UTF("settings.db");
	musik::core::db::Connection db;
    db.Open(dbFile.c_str(),0,128);

    Preferences::CreateDB(db);

	// Get the libraries
	db::Statement stmtGetLibs("SELECT name,type FROM libraries ORDER BY id",db);

	while(stmtGetLibs.Step()==db::Row){
		this->AddLibrary( stmtGetLibs.ColumnTextUTF(0),stmtGetLibs.ColumnInt(1) );
	}

	// If there are no libraries, add a LocalDB
	if(this->libraries.empty()){
		this->CreateLibrary(UTF("Local Library"),Types::LocalDB);
	}

}

LibraryFactory::~LibraryFactory(void){
}

bool LibraryFactory::AddLibrary(utfstring name,int type,bool sendEvent){
	LibraryPtr lib;
	switch(type){
		case Types::Remote:
			lib	= Library::Remote::Create(name);
			break;
		default:
			lib	= Library::LocalDB::Create(name);
	}

	if(lib){
		this->libraries.push_back(lib);

        if(sendEvent){
            this->LibrariesUpdated();
        }

		// Start the library
		return lib->Startup();
	}
    return false;
}

void LibraryFactory::RemoveLibrary(utfstring name){
}

bool LibraryFactory::CreateLibrary(utfstring name,int type){
	// Connect to the settings.db
	utfstring dataDir   = GetDataDirectory();
    utfstring dbFile    = GetDataDirectory() + UTF("settings.db");
	musik::core::db::Connection db;
    db.Open(dbFile.c_str(),0,128);

	db::Statement stmtInsert("INSERT OR FAIL INTO libraries (name,type) VALUES (?,?)",db);
	stmtInsert.BindTextUTF(0,name);
	stmtInsert.BindInt(1,type);
	if(stmtInsert.Step()==db::Done){
		return this->AddLibrary(name,type,true);
	}
	return false;
}

void LibraryFactory::DeleteLibrary(utfstring name){
}

LibraryFactory::LibraryVector& LibraryFactory::Libraries(){
	return LibraryFactory::sInstance.libraries;
}

