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

#include <core/Library/Base.h>
#include <core/Query/Base.h>
#include <core/Common.h>

#include <boost/filesystem.hpp>

using namespace musik::core;

Library::Base::Base(void) : identifier(UTF("local")), queueCallbackStarted(false), bCurrentQueryCanceled(false), exit(false){
}

Library::Base::~Base(void){
    this->Exit(true);
    this->threads.join_all();
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
utfstring Library::Base::GetLibraryDirectory(){
    utfstring directory( musik::core::GetDataDirectory() );

    if(!this->identifier.empty()){
        directory.append(this->identifier+UTF("/"));
    }

    boost::filesystem::utfpath oFolder(directory);
    if(!boost::filesystem::exists(oFolder)){
        boost::filesystem::create_directories(oFolder);
    }

    directory   = oFolder.string();

    return directory;
}

//////////////////////////////////////////
///\brief
///Get the full path to the database file.
///
///\returns
///String with the path
//////////////////////////////////////////
utfstring Library::Base::GetDBPath(){
    utfstring sPath    = this->GetLibraryDirectory();
    sPath.append(UTF("local_musik.db"));
    return sPath;
}




//////////////////////////////////////////
///\brief
///Add a Query for parsing to the Library
///
///\param queryCopy
///Copy of the query that needs to be parsed. Need to be derivated from the Query::Base. Look at musik::core::Query::copy
///
///\param options
///A bitfield with options for the query.
///Available options are:
///    - Query::Options::AutoCallback : The callbacks in the Query should automaticaly be called.
///        Note that if you pass Query::Options::AutoCallback without Query::Options::Wait, the callbacks will be called from the Library thread.
///    - Query::Options::Wait : Wait for the Query to finish executing and then continue.
///    - Query::Options::Prioritize : Will make the library prioritize the Query.
///    - Query::Options::CancelQueue : Cancel all other queries that are to be executed by the Library.
///    - Query::Options::CancelSimilar : Cancel all similar queries. A similar query is a query that originates from the same Query::Base that is passed to the AddQuery.
///    - Query::Options::UnCanceable : Under no circumstances is this Query allowed to be canceled.
///
///The query will be copied by the library and executed in the library thread.
///
///\returns
///true if successfully added to the queue
///
///\see
///musik::core::Query::Base::copy
//////////////////////////////////////////
bool Library::Base::AddQuery( const Query::Base &query,unsigned int options ){


    // Start by making a copy
    Query::Ptr queryCopy( query.copy() );

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
        // Lock the mutex for accessing the query queues
        boost::mutex::scoped_lock lock(this->libraryMutex);

        bool bCancelCurrentQuery(false);

        /////////////////////////////////////////////////////////////////////////////
        // Clear unparsed queue that match CANCEL options
        for(std::list<Query::Ptr>::iterator oCheckQuery=this->incomingQueries.begin();oCheckQuery!=this->incomingQueries.end();){
            // Do not erase UNCANCEABLE
            if( !((*oCheckQuery)->options & Query::Options::UnCanceable) ){
                if( options & Query::Options::CancelQueue ){
                    oCheckQuery    = this->incomingQueries.erase(oCheckQuery);
                }else if( options & Query::Options::CancelSimilar ){
                    if( (*oCheckQuery)->iQueryId == queryCopy->iQueryId ){
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
        for(std::list<Query::Ptr>::iterator oCheckQuery=this->outgoingQueries.begin();oCheckQuery!=this->outgoingQueries.end();++oCheckQuery){

            // Do not erase UNCANCEABLE
            if( !((*oCheckQuery)->options & Query::Options::UnCanceable) ){
                if( options & Query::Options::CancelQueue ){
                    (*oCheckQuery)->status    |= Query::Base::Status::Canceled;
                }else if( options & Query::Options::CancelSimilar ){
                    if( (*oCheckQuery)->iQueryId == queryCopy->iQueryId ){
                        (*oCheckQuery)->status    |= Query::Base::Status::Canceled;
                    }
                }
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Cancel running query
        if(this->runningQuery){
            if( !(this->runningQuery->options & Query::Options::UnCanceable) ){
                if( options & Query::Options::CancelQueue ){
                    bCancelCurrentQuery    = true;
                }else if( options & Query::Options::CancelSimilar ){
                    if( this->runningQuery->iQueryId == queryCopy->iQueryId ){
                        bCancelCurrentQuery    = true;
                    }
                }
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Add the new query to front of incomming queue if the query is prioritized.
        if(options & Query::Options::Prioritize){
            this->incomingQueries.push_front(queryCopy);
        }else{
            this->incomingQueries.push_back(queryCopy);
        }

        /////////////////////////////////////////////////////////////////////////////
        // Cancel currently running query
        if(bCancelCurrentQuery){
            this->CancelCurrentQuery();
        }

    }

    /////////////////////////////////////////////////////////////////////////////
    // Notify library thread that a query has been added.
    this->waitCondition.notify_all();


    /////////////////////////////////////////////////////////////////////////////
    // If the Query::Options::Wait is set, wait for query to finish.
    if(options&Query::Options::Wait){
        {
            boost::mutex::scoped_lock lock(this->libraryMutex);

            // wait for the query to be finished or canceled
            while( !(queryCopy->status&Query::Base::Status::Ended) && !(queryCopy->status&Query::Base::Status::Canceled) ){
                this->waitCondition.wait(lock);
            }

            if( options & Query::Options::AutoCallback ){    // Should the callbacks be involved?
                queryCopy->status    |= Query::Base::Status::Finished;    // Set to finished for removal.
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Finaly, remove old, finished queries
        this->ClearFinishedQueries();

        if( options & Query::Options::AutoCallback ){    // Should the callbacks be involved?
            if( !(queryCopy->status&Query::Base::Status::Canceled) ){    // If not canceled
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
Query::Ptr Library::Base::GetNextQuery(){
    boost::mutex::scoped_lock lock(this->libraryMutex);

    if(this->incomingQueries.size()!=0){
        Query::Ptr oQuery    = this->incomingQueries.front();    // Cast back to query
        this->incomingQueries.pop_front();
        return oQuery;
    }

    // Or return an empty query
    return Query::Ptr();

}

//////////////////////////////////////////
///\brief
///Clears all finished queries from the outgoingQueries
///
///\returns
///Returns true if all queues are finished
//////////////////////////////////////////
bool Library::Base::ClearFinishedQueries(){
    // Remove old queries
    boost::mutex::scoped_lock lock(this->libraryMutex);

    for(std::list<Query::Ptr>::iterator oCheckQuery=this->outgoingQueries.begin();oCheckQuery!=this->outgoingQueries.end();){
        unsigned int status    = (*oCheckQuery)->status;
        if( (status & (Query::Base::Status::Finished | Query::Base::Status::Canceled)) ){
            oCheckQuery    = this->outgoingQueries.erase(oCheckQuery);
        }else{
            ++oCheckQuery;
        }
    }
    return (this->incomingQueries.size()==0 && this->outgoingQueries.size()==0);
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
bool Library::Base::RunCallbacks(){

    this->ClearFinishedQueries();

    bool bAgain(true);

    while(bAgain){
        Query::Ptr oQuery;
        bAgain    = false;

        {
            boost::mutex::scoped_lock lock(this->libraryMutex);

            if(this->outgoingQueries.size()!=0){
                oQuery    = this->outgoingQueries.front();
                if(oQuery->options & Query::Options::AutoCallback){
                    oQuery.reset();
                }
            }
        }

        if(oQuery){
            if(oQuery->RunCallbacks(this)){
                boost::mutex::scoped_lock lock(this->libraryMutex);
                // Set to FINISHED if query returns true
                oQuery->status    |= Query::Base::Status::Finished;
                bAgain    = true;    // Continue to check results on the rest of the queue if this one is finished.
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
bool Library::Base::QueryCanceled(){
    boost::mutex::scoped_lock lock(this->libraryMutex);
    bool bReturn    = this->bCurrentQueryCanceled;
    this->bCurrentQueryCanceled    = false;
    return bReturn;
}

//////////////////////////////////////////
///\brief
///Cancel the current running query
///
///\remarks
///This method is virtual so that a library may do more to stop the query.
///For instance a database library may try to interrupt the current running SQL-query.
///
///\see
///QueryCanceled
//////////////////////////////////////////
void Library::Base::CancelCurrentQuery(){
    boost::mutex::scoped_lock lock(this->libraryMutex);
    this->bCurrentQueryCanceled    = true;
}


bool Library::Base::Exit(void){
    boost::mutex::scoped_lock lock(this->libraryMutex);
    return this->exit;
}

void Library::Base::Exit(bool exit){
    {
        boost::mutex::scoped_lock lock(this->libraryMutex);
        this->exit    = exit;
    }
    this->waitCondition.notify_all();
}


bool Library::Base::IsStaticMetaKey(std::string &metakey){
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

bool Library::Base::IsSpecialMTOMetaKey(std::string &metakey){
    static std::set<std::string> specialMTOMetaKeys;

    if(specialMTOMetaKeys.empty()){
        specialMTOMetaKeys.insert("album");
        specialMTOMetaKeys.insert("visual_genre");
        specialMTOMetaKeys.insert("visual_artist");
        specialMTOMetaKeys.insert("folder");
    }
    return specialMTOMetaKeys.find(metakey)!=specialMTOMetaKeys.end();
}

bool Library::Base::IsSpecialMTMMetaKey(std::string &metakey){
    static std::set<std::string> specialMTMMetaKeys;

    if(specialMTMMetaKeys.empty()){
        specialMTMMetaKeys.insert("artist");
        specialMTMMetaKeys.insert("genre");
    }
    return specialMTMMetaKeys.find(metakey)!=specialMTMMetaKeys.end();
}


