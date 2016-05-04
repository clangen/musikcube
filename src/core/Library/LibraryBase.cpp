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

#include <core/Library/LibraryBase.h>
#include <core/config.h>
#include <core/library/query/QueryBase.h>
#include <core/Common.h>

using namespace musik::core;
using namespace musik::core::library;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
LibraryBase::LibraryBase(std::string name,int id)
 :name(name)
 ,id(id)
 ,queueCallbackStarted(false)
 ,exit(false)
 ,userId(1)
{
    this->identifier = boost::lexical_cast<std::string>(id);
}

//////////////////////////////////////////
///\brief
///Destructor
///
///The destructor will exit all threads created by the library
//////////////////////////////////////////
LibraryBase::~LibraryBase(void){
    this->Exit();
    this->threads.join_all();
}

//////////////////////////////////////////
///\brief
///Get the librarys identifier. The identifier is unique for the library
///
///\returns
///A string with the identifier
//////////////////////////////////////////
const std::string& LibraryBase::Identifier(){
	return this->identifier;
}

int LibraryBase::Id(){
    return this->id;
}

//////////////////////////////////////////
///\brief
///Name of the library
//////////////////////////////////////////
const std::string& LibraryBase::Name(){
	return this->name;
}

//////////////////////////////////////////
///\brief
///Get the directory-location of the library where you may store extra files.
///
///\returns
///String with the path
///
///The library directory is a directory where you may store
///the librarys database and other files like thumbnail cache.
///In a win32 environment this path will be located in the users
///$APPDATA/mC2/"identifier"/
///where the identifier is set in the library itself.
///
///\remarks
///If the directory does not exist, this method will create it.
//////////////////////////////////////////
std::string LibraryBase::GetLibraryDirectory(){
    std::string directory( musik::core::GetDataDirectory() );

    if(!this->identifier.empty()){
        directory.append(this->identifier+"/");
    }

    boost::filesystem::path oFolder(directory);
    if(!boost::filesystem::exists(oFolder)){
        boost::filesystem::create_directories(oFolder);
    }

    directory = oFolder.string();

    return directory;
}

//////////////////////////////////////////
///\brief
///Get the full path to the database file.
///
///\returns
///String with the path
//////////////////////////////////////////
std::string LibraryBase::GetDBPath(){
    std::string sPath = this->GetLibraryDirectory();
    sPath.append("musik.db");
    return sPath;
}

//////////////////////////////////////////
///\brief
///Add a Query for parsing to the Library
///
///\param queryCopy
///Copy of the query that needs to be parsed. Need to be derivated from the query::QueryBase. Look at musik::core::query::copy
///
///\param options
///A bitfield with options for the query.
///Available options are:
///    - query::Options::AutoCallback : The callbacks in the Query should automaticaly be called.
///        Note that if you pass query::Options::AutoCallback without query::Options::Wait, the callbacks will be called from the Library thread.
///    - query::Options::Wait : Wait for the Query to finish executing and then continue.
///    - query::Options::Prioritize : Will make the library prioritize the Query.
///    - query::Options::CancelQueue : Cancel all other queries that are to be executed by the Library.
///    - query::Options::CancelSimilar : Cancel all similar queries. A similar query is a query that originates from the same query::QueryBase that is passed to the AddQuery.
///    - query::Options::UnCanceable : Under no circumstances is this Query allowed to be canceled.
///    - query::Options::UnCanceable : Under no circumstances is this Query allowed to be canceled.
///
///The query will be copied by the library and executed in the library thread.
///
///\returns
///true if successfully added to the queue
///
///\see
///musik::core::query::QueryBase::copy
//////////////////////////////////////////
bool LibraryBase::AddQuery( const query::QueryBase &query,unsigned int options ){

	if(this->Exited()){
		return false;
	}

    // Start by making a copy
    query::Ptr queryCopy( query.copy() );

    // 
    if(options&query::CopyUniqueId){
        queryCopy->uniqueId = query.uniqueId;
    }

    queryCopy->PreAddQuery(this);

    // Since query is not added to queue yet, variables can now be changed without locking.

    // Set the options in the query. They should not be altered and are therefore threadsafe.
    queryCopy->options    = options;

    // From here on mutexes should be used

    /////////////////////////////////////////////////////////////////////////////
    // First lets check if the OnQueryQueueStart should be called.
    // No mutexes should be locked when this is called or deadlock might occure
    bool queueStart(false);
    {
        // Start by checking
        boost::mutex::scoped_lock lock(this->libraryMutex);

        if( !this->queueCallbackStarted ){
            this->queueCallbackStarted = true;
            queueStart        = true;
        }
    }
    if(queueStart){
        this->OnQueryQueueStart();
    }

    /////////////////////////////////////////////////////////////////////////////
    {
        // Lock the mutex for accessing the query queues and query status
        boost::mutex::scoped_lock lock(this->libraryMutex);

        bool cancelCurrentQuery(false);

        /////////////////////////////////////////////////////////////////////////////
        // Clear unparsed queue that match CANCEL options
        for(std::list<query::Ptr>::iterator oCheckQuery=this->incomingQueries.begin();oCheckQuery!=this->incomingQueries.end();){
            // Do not erase UNCANCEABLE
            if( !((*oCheckQuery)->options & query::UnCanceable) ){
                if( options & query::CancelQueue ){
                    oCheckQuery    = this->incomingQueries.erase(oCheckQuery);
                }else if( options & query::CancelSimilar ){
                    if( (*oCheckQuery)->queryId == queryCopy->queryId ){
                        oCheckQuery    = this->incomingQueries.erase(oCheckQuery);
                    }else{
                        ++oCheckQuery;
                    }
                }else{
                    ++oCheckQuery;
                }
            }else{
                ++oCheckQuery;
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Even cancel parsed queries
        for(std::list<query::Ptr>::iterator oCheckQuery=this->outgoingQueries.begin();oCheckQuery!=this->outgoingQueries.end();++oCheckQuery){

            // Do not erase UNCANCEABLE
            if( !((*oCheckQuery)->options & query::UnCanceable) ){
                if( options & query::CancelQueue ){
                    (*oCheckQuery)->status    |= query::QueryBase::Canceled;
                }else if( options & query::CancelSimilar ){
                    if( (*oCheckQuery)->queryId == queryCopy->queryId ){
                        (*oCheckQuery)->status    |= query::QueryBase::Canceled;
                    }
                }
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Cancel running query
        if(this->runningQuery){
            if( !(this->runningQuery->options & query::UnCanceable) ){
                if( options & query::CancelQueue ){
                    cancelCurrentQuery    = true;
                }else if( options & query::CancelSimilar ){
                    if( this->runningQuery->queryId == queryCopy->queryId ){
                        cancelCurrentQuery    = true;
                    }
                }
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Add the new query to front of incomming queue if the query is prioritized.
        if(options & query::Prioritize){
            this->incomingQueries.push_front(queryCopy);
        }else{
            this->incomingQueries.push_back(queryCopy);
        }

        /////////////////////////////////////////////////////////////////////////////
        // Cancel currently running query
        if(cancelCurrentQuery){
            this->CancelCurrentQuery();
        }

    }


    /////////////////////////////////////////////////////////////////////////////
    // Notify library thread that a query has been added.
    this->waitCondition.notify_all();


    /////////////////////////////////////////////////////////////////////////////
    // If the query::Options::Wait is set, wait for query to finish.
    if(options&query::Wait){
        {
            boost::mutex::scoped_lock lock(this->libraryMutex);

            // wait for the query to be finished or canceled
            while( !(queryCopy->status&query::QueryBase::Ended) && !(queryCopy->status&query::QueryBase::Canceled) ){

                // To be on the safe side, lets check every second
                boost::xtime waitingTime;
                boost::xtime_get(&waitingTime, boost::TIME_UTC_);
                waitingTime.sec += 1;
                this->waitCondition.timed_wait(lock,waitingTime);
            }

            if( options & query::AutoCallback ){    // Should the callbacks be involved?
                queryCopy->status    |= query::QueryBase::Finished;    // Set to finished for removal.
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Finaly, remove old, finished queries
        this->ClearFinishedQueries();

        if( options & query::AutoCallback ){    // Should the callbacks be involved?
            if( !this->QueryCanceled(queryCopy.get()) ){    // If not canceled
                queryCopy->RunCallbacks(this);            // Run the callbacks.
            }
        }


    }

    return true;
}


//////////////////////////////////////////
///\brief
///Pull the next query from the incomingQueries
//////////////////////////////////////////
query::Ptr LibraryBase::GetNextQuery(){
    boost::mutex::scoped_lock lock(this->libraryMutex);

    if(this->incomingQueries.size()!=0){
        query::Ptr query    = this->incomingQueries.front();    // Cast back to query
        this->incomingQueries.pop_front();
        return query;
    }

    // Or return an empty query
    return query::Ptr();

}

//////////////////////////////////////////
///\brief
///Clears all finished queries from the outgoingQueries
///
///\returns
///Returns true if all queues are finished
//////////////////////////////////////////
bool LibraryBase::ClearFinishedQueries(){
    std::vector<query::Ptr> canceledQueries;

    {
        // Remove old queries
        boost::mutex::scoped_lock lock(this->libraryMutex);

        for(std::list<query::Ptr>::iterator oCheckQuery=this->outgoingQueries.begin();oCheckQuery!=this->outgoingQueries.end();){
            unsigned int status    = (*oCheckQuery)->status;
            if( (status & (query::QueryBase::Finished | query::QueryBase::Canceled)) ){
                // If canceled
                if( status & query::QueryBase::Canceled ){
                    canceledQueries.push_back(*oCheckQuery);
                }
                oCheckQuery    = this->outgoingQueries.erase(oCheckQuery);
            }else{
                ++oCheckQuery;
            }
        }
    }

    // Lets notify queries that they have been canceled
    for(std::vector<query::Ptr>::iterator query=canceledQueries.begin();query!=canceledQueries.end();++query){
        (*query)->QueryFinished(query->get(),this,false);
    }

    {
        boost::mutex::scoped_lock lock(this->libraryMutex);
        return (this->incomingQueries.size()==0 && this->outgoingQueries.size()==0);
    }
}

//////////////////////////////////////////
///\brief
///Call the callbacks (signals) for finished queries.
///
///\returns
///true when there is nothing else to call.
///
///Write detailed description for RunCallbacks here.
///
///\see
///OnQueryQueueEnd
//////////////////////////////////////////
bool LibraryBase::RunCallbacks(){

    this->ClearFinishedQueries();

    bool bAgain(true);

    while(bAgain){
        query::Ptr oQuery;
        bAgain    = false;

        {
            boost::mutex::scoped_lock lock(this->libraryMutex);

            if(this->outgoingQueries.size()!=0){
                oQuery    = this->outgoingQueries.front();
                if(oQuery->options & query::AutoCallback){
                    oQuery.reset();
                }
            }
        }

        if(oQuery){
            if(oQuery->RunCallbacks(this)){
                {
                    boost::mutex::scoped_lock lock(this->libraryMutex);
                    // Set to FINISHED if query returns true
                    oQuery->status    |= query::QueryBase::Finished;
                }
                bAgain    = true;    // Continue to check results on the rest of the queue if this one is finished.

                // Call the query signal that the query is finished
                oQuery->QueryFinished(oQuery.get(),this,true);
            }
        }
        this->ClearFinishedQueries();

    }

    if(this->ClearFinishedQueries()){
        this->queueCallbackStarted  = false;
        this->OnQueryQueueEnd();
        return true;
    }

    return false;
}

//////////////////////////////////////////
///\brief
///Check if the current running query is canceled.
///
///\returns
///True if canceled.
///
///This method is used by the Queries to check if they are canceled
///while they are running.
///
///\remarks
///This memthod is threadsafe
///
///\see
///CancelCurrentQuery
//////////////////////////////////////////
bool LibraryBase::QueryCanceled(query::QueryBase *query){
    boost::mutex::scoped_lock lock(this->libraryMutex);
	return (query->status&query::QueryBase::Canceled)?true:false;
}

//////////////////////////////////////////
///\brief
///Cancel the current running query
///
///\remarks
///This method is virtual so that a library may do more to stop the query.
///For instance a database library may try to interrupt the current running SQL-query.
///This method assumes that the libraryMutex is locked.
///
///\see
///QueryCanceled
//////////////////////////////////////////
void LibraryBase::CancelCurrentQuery(){
//    this->bCurrentQueryCanceled    = true;
}


//////////////////////////////////////////
///\brief
///Has the library exited?
//////////////////////////////////////////
bool LibraryBase::Exited(){
    boost::mutex::scoped_lock lock(this->libraryMutex);
    return this->exit;
}

//////////////////////////////////////////
///\brief
///Exit the library
///
///Will set the library to Exited and notify all sleeping threads
//////////////////////////////////////////
void LibraryBase::Exit(){
    {
        boost::mutex::scoped_lock lock(this->libraryMutex);
        this->exit    = true;
    }
    this->waitCondition.notify_all();
}


//////////////////////////////////////////
///\brief
///Helper method to determin what metakeys are "static"
//////////////////////////////////////////
bool LibraryBase::IsStaticMetaKey(std::string &metakey){
    static std::set<std::string> staticMetaKeys;

    if(staticMetaKeys.empty()){
        staticMetaKeys.insert("track");
        staticMetaKeys.insert("bpm");
        staticMetaKeys.insert("duration");
        staticMetaKeys.insert("filesize");
        staticMetaKeys.insert("year");
        staticMetaKeys.insert("title");
        staticMetaKeys.insert("filename");
        staticMetaKeys.insert("filetime");
    }

    return staticMetaKeys.find(metakey)!=staticMetaKeys.end();

}

//////////////////////////////////////////
///\brief
///Helper method to determin what metakeys that have a special many to one relation
//////////////////////////////////////////
bool LibraryBase::IsSpecialMTOMetaKey(std::string &metakey){
    static std::set<std::string> specialMTOMetaKeys;

    if(specialMTOMetaKeys.empty()){
        specialMTOMetaKeys.insert("album");
        specialMTOMetaKeys.insert("visual_genre");
        specialMTOMetaKeys.insert("visual_artist");
        specialMTOMetaKeys.insert("folder");
    }
    return specialMTOMetaKeys.find(metakey)!=specialMTOMetaKeys.end();
}

//////////////////////////////////////////
///\brief
///Helper method to determin what metakeys that have a special many to meny relation
//////////////////////////////////////////
bool LibraryBase::IsSpecialMTMMetaKey(std::string &metakey){
    static std::set<std::string> specialMTMMetaKeys;

    if(specialMTMMetaKeys.empty()){
        specialMTMMetaKeys.insert("artist");
        specialMTMMetaKeys.insert("genre");
    }
    return specialMTMMetaKeys.find(metakey)!=specialMTMMetaKeys.end();
}

//////////////////////////////////////////
///\brief
///Get a pointer to the librarys Indexer (NULL if none)
//////////////////////////////////////////
musik::core::Indexer *LibraryBase::Indexer(){
    return NULL;
}


//////////////////////////////////////////
///\brief
///Create all tables, indexes, etc in the database.
///
///This will assume that the database has been initialized.
//////////////////////////////////////////
void LibraryBase::CreateDatabase(db::Connection &db){
    // Create the tracks-table
    db.Execute("CREATE TABLE IF NOT EXISTS tracks ("
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
    db.Execute("CREATE TABLE IF NOT EXISTS genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    db.Execute("CREATE TABLE IF NOT EXISTS track_genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "genre_id INTEGER DEFAULT 0)");

    // Create the artists-table
    db.Execute("CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    db.Execute("CREATE TABLE IF NOT EXISTS track_artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "artist_id INTEGER DEFAULT 0)");

    // Create the meta-tables
    db.Execute("CREATE TABLE IF NOT EXISTS meta_keys ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT)");

    db.Execute("CREATE TABLE IF NOT EXISTS meta_values ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "meta_key_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0,"
            "content TEXT)");

    db.Execute("CREATE TABLE IF NOT EXISTS track_meta ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "meta_value_id INTEGER DEFAULT 0)");

    // Create the albums-table
    db.Execute("CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "thumbnail_id INTEGER default 0,"
            "sort_order INTEGER DEFAULT 0)");

    // Create the paths-table
    db.Execute("CREATE TABLE IF NOT EXISTS paths ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "path TEXT default ''"
            ")");

    // Create the folders-table
    db.Execute("CREATE TABLE IF NOT EXISTS folders ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "relative_path TEXT default '',"
            "parent_id INTEGER DEFAULT 0,"
            "path_id INTEGER DEFAULT 0"
            ")");

    // Create the folders-table
    db.Execute("CREATE TABLE IF NOT EXISTS thumbnails ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "filename TEXT default '',"
            "filesize INTEGER DEFAULT 0,"
            "checksum INTEGER DEFAULT 0"
            ")");

    // Create the playlists-table
    db.Execute("CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "user_id INTEGER default 0"
            ")");
    // Create the playlists-table
    db.Execute("CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "track_id INTEGER DEFAULT 0,"
            "playlist_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0"
            ")");

    // Create the users-table
    db.Execute("CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "login TEXT,"
        "password TEXT)");

    // INDEXES
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS users_index ON users (login)");
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS folders_index ON folders (name,parent_id,path_id)");
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS paths_index ON paths (path)");
    db.Execute("CREATE INDEX IF NOT EXISTS genre_index ON genres (sort_order)");
    db.Execute("CREATE INDEX IF NOT EXISTS artist_index ON artists (sort_order)");
    db.Execute("CREATE INDEX IF NOT EXISTS album_index ON albums (sort_order)");
    db.Execute("CREATE INDEX IF NOT EXISTS track_index1 ON tracks (album_id,sort_order1)");
    db.Execute("CREATE INDEX IF NOT EXISTS track_index7 ON tracks (folder_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS thumbnail_index ON thumbnails (filesize)");

    db.Execute("CREATE INDEX IF NOT EXISTS trackgenre_index1 ON track_genres (track_id,genre_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS trackgenre_index2 ON track_genres (genre_id,track_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS trackartist_index1 ON track_artists (track_id,artist_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS trackartist_index2 ON track_artists (artist_id,track_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS trackmeta_index1 ON track_meta (track_id,meta_value_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS trackmeta_index2 ON track_meta (meta_value_id,track_id)");
    db.Execute("CREATE INDEX IF NOT EXISTS metakey_index1 ON meta_keys (name)");
    db.Execute("CREATE INDEX IF NOT EXISTS metavalues_index1 ON meta_values (meta_key_id)");

    db.Execute("CREATE INDEX IF NOT EXISTS playlist_index ON playlist_tracks (playlist_id,sort_order)");

    db.Analyze();
}

//////////////////////////////////////////
///\brief
///Get the base path to where the tracks are located
///
///This method is mostly used by the Remote to
///get the HTTP-address to the tracks
//////////////////////////////////////////
std::string LibraryBase::BasePath(){
    return "";
}

LibraryPtr LibraryBase::GetSelfPtr(){
    if(LibraryPtr thisPtr = this->self.lock()){
        return thisPtr;
    }
    return LibraryPtr();
}

const std::string& LibraryBase::AuthorizationKey(){
    static std::string emptyAuthString;
    return emptyAuthString;
}

