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
#include <core/Library/LocalDB.h>
#include <core/Query/Base.h>

#include <boost/bind.hpp>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor.
///
///The constructor will not start the Library.
///
///\see
///Startup
//////////////////////////////////////////
Library::LocalDB::LocalDB(void){
}

Library::LocalDB::~LocalDB(void){
}

//////////////////////////////////////////
///\brief
///Get a short status string of what is going on in the Library.
///
///\returns
///Information string.
///
///The information is mostly used to get the information
///about the Indexer.
//////////////////////////////////////////
utfstring Library::LocalDB::GetInfo(){
    return this->indexer.GetStatus();
}


//////////////////////////////////////////
///\brief
///Startup the library threads.
///
///\returns
///True if successfully started. This should always be true. Nothing else is expected.
///
///Start up the Library like this:
///\code
/// // Create a library
/// musik::core::Library::LocalDB library;
/// // Start the library (and indexer that is included)
/// library.Startup();
/// // The library is now ready to recieve queries
///\endcode
//////////////////////////////////////////
bool Library::LocalDB::Startup(){

    // Start the library thread
    this->threads.create_thread(boost::bind(&Library::LocalDB::ThreadLoop,this));

    return true;
}


//////////////////////////////////////////
///\brief
///Create all tables, indexes, etc in the database.
///
///This will assume that the database has been initialized.
//////////////////////////////////////////
void Library::LocalDB::CreateDatabase(){
    // Create the tracks-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS tracks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track INTEGER DEFAULT 0,"
            "bpm REAL DEFAULT 0,"
            "duration INTEGER DEFAULT 0,"
            "filesize INTEGER DEFAULT 0,"
            "year INTEGER DEFAULT 0,"
            "visual_genre_id INTEGER DEFAULT 0,"
            "visual_artist_id INTEGER DEFAULT 0,"
            "album_id INTEGER DEFAULT 0,"
            "folder_id INTEGER DEFAULT 0,"
            "title TEXT default '',"
            "filename TEXT default '',"
            "filetime INTEGER DEFAULT 0,"
            "thumbnail_id INTEGER DEFAULT 0,"
            "sort_order1 INTEGER)");

    // Create the genres-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS track_genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "genre_id INTEGER DEFAULT 0)");

    // Create the artists-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS track_artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "artist_id INTEGER DEFAULT 0)");

    // Create the meta-tables
    this->db.Execute("CREATE TABLE IF NOT EXISTS meta_keys ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS meta_values ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "meta_key_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0,"
            "content TEXT)");

    this->db.Execute("CREATE TABLE IF NOT EXISTS track_meta ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "meta_value_id INTEGER DEFAULT 0)");

    // Create the albums-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "thumbnail_id INTEGER default 0,"
            "sort_order INTEGER DEFAULT 0)");

    // Create the paths-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS paths ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "path TEXT default ''"
            ")");

    // Create the folders-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS folders ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "fullpath TEXT default '',"
            "parent_id INTEGER DEFAULT 0,"
            "path_id INTEGER DEFAULT 0"
            ")");

    // Create the folders-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS thumbnails ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "filename TEXT default '',"
            "filesize INTEGER DEFAULT 0,"
            "checksum INTEGER DEFAULT 0"
            ")");

    // Create the playlists-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default ''"
            ")");
    // Create the playlists-table
    this->db.Execute("CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "track_id INTEGER DEFAULT 0,"
            "playlist_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0"
            ")");


    // INDEXES
    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS folders_index ON folders (name,parent_id,path_id)");
    this->db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS paths_index ON paths (path)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS genre_index ON genres (sort_order)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS artist_index ON artists (sort_order)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS album_index ON albums (sort_order)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS track_index1 ON tracks (album_id,sort_order1)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS track_index7 ON tracks (folder_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS thumbnail_index ON thumbnails (filesize)");

    this->db.Execute("CREATE INDEX IF NOT EXISTS trackgenre_index1 ON track_genres (track_id,genre_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS trackgenre_index2 ON track_genres (genre_id,track_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS trackartist_index1 ON track_artists (track_id,artist_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS trackartist_index2 ON track_artists (artist_id,track_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS trackmeta_index1 ON track_meta (track_id,meta_value_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS trackmeta_index2 ON track_meta (meta_value_id,track_id)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS metakey_index1 ON meta_keys (name)");
    this->db.Execute("CREATE INDEX IF NOT EXISTS metavalues_index1 ON meta_values (meta_key_id)");

    this->db.Execute("CREATE INDEX IF NOT EXISTS playlist_index ON playlist_tracks (playlist_id,sort_order)");

    this->db.Execute("ANALYZE");
}


//////////////////////////////////////////
///\brief
///Main loop the library thread is running in.
///
///The loop will run until Exit(true) has been called.
//////////////////////////////////////////
void Library::LocalDB::ThreadLoop(){

    utfstring database(this->GetDBPath());
    this->db.Open(database.c_str(),0,4096);

    this->CreateDatabase();

    // Startup the indexer
    this->indexer.database    = database;
    this->indexer.Startup(this->GetLibraryDirectory());

    while(!this->Exit()){
        Query::Ptr query(this->GetNextQuery());

        if(query){    // No empty query

            ////////////////////////////////////////////////////////////
            // Add to the finished queries
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                this->bCurrentQueryCanceled    = false;
                this->runningQuery    = query;
                this->outgoingQueries.push_back(query);

                // Set query as started
                query->status |= Query::Base::Status::Started;
            }

            ////////////////////////////////////////////////////////////
            // Lets parse the query
            query->ParseQuery(this,this->db);
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                this->runningQuery.reset();
                // And set it as finished
                query->status |= Query::Base::Status::Ended;
            }

            ////////////////////////////////////////////////////////////
            // Notify that the Query is finished.
            this->waitCondition.notify_all();

        }else{

            ////////////////////////////////////////////////////////////
            // Tricky part, waiting for queries to be added.
            // Not sure I'm doing this the right way.
            // Could this part lead to a deadlock???
            boost::mutex::scoped_lock lock(this->libraryMutex);
            if(!this->exit && this->incomingQueries.size()==0 ){
                this->waitCondition.wait(lock);
            }
        }
    }

}

//////////////////////////////////////////
///\brief
///Cancel the current running query
///
///This method will also send a sqlite3_interrupt to cancel the
///current running SQL Query
//////////////////////////////////////////
void Library::LocalDB::CancelCurrentQuery( ){
    boost::mutex::scoped_lock lock(this->libraryMutex);
    this->bCurrentQueryCanceled    = true;
    sqlite3_interrupt(this->db.connection);
}



