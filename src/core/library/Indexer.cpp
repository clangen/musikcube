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

#include <core/debug.h>
#include <core/library/Indexer.h>

#include <core/config.h>
#include <core/config.h>
#include <core/library/track/IndexerTrack.h>
#include <core/library/track/LibraryTrack.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/plugin/PluginFactory.h>
#include <core/support/Preferences.h>
#include <core/sdk/IAnalyzer.h>
#include <core/audio/Stream.h>

#include <boost/thread/xtime.hpp>
#include <boost/bind.hpp>

static const std::string TAG = "Indexer";

using namespace musik::core;
using namespace musik::core::metadata;
using namespace musik::core::audio;

static std::string normalizeDir(std::string path) {
    path = boost::filesystem::path(path).make_preferred().string();

    std::string sep(1, boost::filesystem::path::preferred_separator);
    if (path.substr(path.size() - 1, 1) != sep) {
        path += sep;
    }

    return path;
}

static std::string normalizePath(const std::string& path) {
    return boost::filesystem::path(path).make_preferred().string();
}

Indexer::Indexer(const std::string& libraryPath, const std::string& dbFilename)
: thread(NULL)
, status(0)
, restart(false)
, filesIndexed(0)
, filesSaved(0) {
    this->dbFilename = dbFilename;
    this->libraryPath = libraryPath;
    this->thread = new boost::thread(boost::bind(&Indexer::ThreadLoop, this));
}

//////////////////////////////////////////
///\brief
///Destructor
///
///Exits and joins threads
//////////////////////////////////////////
Indexer::~Indexer(){
    if (this->thread) {
        this->Exit();
        this->thread->join();
        delete this->thread;
        this->thread = NULL;
    }
}

//////////////////////////////////////////
///\brief
///Restart the sync
///
///\param bNewRestart
///Should if be restarted or not
//////////////////////////////////////////
void Indexer::Synchronize(bool restart) {
    boost::mutex::scoped_lock lock(this->exitMutex);
    this->restart = restart;
    this->Notify();
}

//////////////////////////////////////////
///\brief
///Should the sync be restarted?
//////////////////////////////////////////
bool Indexer::Restarted() {
    boost::mutex::scoped_lock lock(this->exitMutex);
    return this->restart;
}

//////////////////////////////////////////
///\brief
///Add a new path to the Indexer.
///
///\param sPath
///Path to add
///
///\remarks
///If the path already exists it will not be added.
//////////////////////////////////////////
void Indexer::AddPath(const std::string& path) {
    Indexer::AddRemoveContext context;
    context.add = true;
    context.path = normalizeDir(path);

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(context);
    }

    this->Synchronize();
}

//////////////////////////////////////////
///\brief
///Remove a path from the Indexer
///
///\param sPath
///Path to remove
//////////////////////////////////////////
void Indexer::RemovePath(const std::string& path) {
    Indexer::AddRemoveContext context;
    context.add = false;
    context.path = normalizeDir(path);

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(context);
    }

    this->Synchronize();
}

//////////////////////////////////////////
///\brief
///Main method for doing the synchronization.
//////////////////////////////////////////
void Indexer::SynchronizeInternal() {
    /* load all of the metadata (tag) reader plugins */
    typedef PluginFactory::DestroyDeleter<IMetadataReader> MetadataDeleter;
    typedef PluginFactory::DestroyDeleter<IDecoderFactory> DecoderDeleter;

    this->metadataReaders = PluginFactory::Instance()
        .QueryInterface<IMetadataReader, MetadataDeleter>("GetMetadataReader");

    this->audioDecoders = PluginFactory::Instance()
        .QueryInterface<IDecoderFactory, DecoderDeleter>("GetDecoderFactory");

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->filesIndexed = 0;
    }

    this->ProcessAddRemoveQueue();

    /* get sync paths and ids from the db */

    std::vector<std::string> paths;
    std::vector<DBID> pathIds;

    {
        db::Statement stmt("SELECT id, path FROM paths", this->dbConnection);

        while (stmt.Step() == db::Row) {
            try {
                DBID id = stmt.ColumnInt(0);
                std::string path = stmt.ColumnText(1);
                boost::filesystem::path dir(path);

                if (boost::filesystem::exists(dir)) {
                    paths.push_back(path);
                    pathIds.push_back(id);
                }
            }
            catch(...) {
            }
        }
    }

    /* add new files  */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 2;
        this->filesSaved = 0;
    }

    for(std::size_t i = 0; i < paths.size(); ++i) {
        std::string path = paths[i];
        this->SyncDirectory(path, path, pathIds[i]);
    }

    /* remove undesired entries from db (files themselves will remain) */

    musik::debug::info(TAG, "cleanup 1/2");

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 3;
    }

    if (!this->Restarted() && !this->Exited()) {
        this->SyncDelete();
    }

    /* cleanup -- remove stale artists, albums, genres, etc */

    musik::debug::info(TAG, "cleanup 2/2");

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 4;
    }


    if(!this->Restarted() && !this->Exited()){
        this->SyncCleanup();
    }

    /* optimize */

    musik::debug::info(TAG, "optimizing");

    {
       boost::mutex::scoped_lock lock(this->progressMutex);
       this->status = 5;
    }

    if(!this->Restarted() && !this->Exited()){
        this->SyncOptimize();
    }

    /* unload reader DLLs*/

    this->metadataReaders.clear();

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 0;
    }

    musik::debug::info(TAG, "done!");
}

void Indexer::SyncDirectory(
    const std::string &syncRoot,
    const std::string &currentPath,
    DBID pathId)
{
    if (this->Exited() || this->Restarted()) {
        return;
    }

    std::string normalizedSyncRoot = normalizeDir(syncRoot);
    std::string normalizedCurrentPath = normalizeDir(currentPath);
    std::string leaf = boost::filesystem::path(currentPath).leaf().string(); /* trailing subdir in currentPath */

    /* start recursive filesystem scan */

    try { /* boost::filesystem may throw */

        /* for each file in the current path... */

        boost::filesystem::path path(currentPath);
        boost::filesystem::directory_iterator end;
        boost::filesystem::directory_iterator file(path);

        std::string pathIdStr = boost::lexical_cast<std::string>(pathId);

        for( ; file != end && !this->Exited() && !this->Restarted(); file++) {
            if (is_directory(file->status())) {
                /* recursion here */
                musik::debug::info(TAG, "scanning " + file->path().string());
                this->SyncDirectory(syncRoot, file->path().string(), pathId);
            }
            else {
                ++this->filesIndexed;

                musik::core::IndexerTrack track(0);

                /* get cached filesize, parts, size, etc */
                if (track.NeedsToBeIndexed(file->path(), this->dbConnection)) {
                    bool saveToDb = false;

                    /* read the tag from the plugin */
                    typedef MetadataReaderList::iterator Iterator;
                    Iterator it = this->metadataReaders.begin();
                    while (it != this->metadataReaders.end()) {
                        if ((*it)->CanRead(track.GetValue("extension").c_str())) {
                            if ((*it)->Read(file->path().string().c_str(), &track)) {
                                saveToDb = true;
                                break;
                            }
                        }
                        it++;
                    }

                    /* no tag? well... if a decoder can play it, add it to the database
                    with the file as the name. */
                    if (!saveToDb) {
                        std::string fullPath = file->path().string();
                        auto it = this->audioDecoders.begin();
                        while (it != this->audioDecoders.end()) {
                            if ((*it)->CanHandle(fullPath.c_str())) {
                                saveToDb = true;
                                track.SetValue("title", file->path().leaf().string().c_str());
                                break;
                            }
                            ++it;
                        }
                    }

                    /* write it to the db, if read successfully */
                    if (saveToDb) {
                        track.SetValue("path_id", pathIdStr.c_str());
                        track.Save(this->dbConnection, this->libraryPath);

                        this->filesSaved++;
                        if (this->filesSaved % 100 == 0) {
                            this->TrackRefreshed(); /* no idea... something listens to this. maybe?*/
                        }
                    }
                }
            }
        }
    }
    catch(...) {
    }
}

//////////////////////////////////////////
///\brief
///Main loop the thread is running in.
//////////////////////////////////////////
void Indexer::ThreadLoop() {
    boost::filesystem::path thumbPath(this->libraryPath + "thumbs/");

    if (!boost::filesystem::exists(thumbPath)) {
        boost::filesystem::create_directories(thumbPath);
    }

    bool firstTime = true; /* through the loop */

    while (!this->Exited()) {
        this->restart = false;

        Preferences prefs("Indexer");

        if(!firstTime || (firstTime && prefs.GetBool("SyncOnStartup", true))) { /* first time through the loop skips this */
            this->SynchronizeStart();

            this->dbConnection.Open(this->dbFilename.c_str(), 0); /* ensure the db is open*/

            this->SynchronizeInternal();
            this->RunAnalyzers();

            {
                boost::mutex::scoped_lock lock(this->progressMutex);
                this->status = 0;
            }

            this->dbConnection.Close(); /* TODO: raii */

            this->SynchronizeEnd();
        } /* end skip */

        firstTime = false;

        int waitTime = prefs.GetInt("SyncTimeout", 3600); /* sleep before we try again... */

        if (waitTime) {
            boost::xtime waitTimeout;
            boost::xtime_get(&waitTimeout, boost::TIME_UTC_);
            waitTimeout.sec += waitTime;

            if (!this->Restarted()) {
                this->NotificationTimedWait(waitTimeout);
            }
        }
        else {
            if (!this->Restarted()) {
                this->NotificationWait(); /* zzz */
            }
        }
    }
}

void Indexer::SyncDelete() {
    /* remove all tracks that no longer reference a valid path entry */

    this->dbConnection.Execute("DELETE FROM tracks WHERE path_id NOT IN (SELECT id FROM paths)");

    /* remove files that are no longer on the filesystem. */

    db::Statement stmtRemove("DELETE FROM tracks WHERE id=?", this->dbConnection);

    db::Statement allTracks(
        "SELECT t.id, t.filename "
        "FROM tracks t ", this->dbConnection);

    while(allTracks.Step() == db::Row && !this->Exited() && !this->Restarted()) {
        bool remove = false;
        std::string fn = allTracks.ColumnText(1);

        try {
            boost::filesystem::path file(fn);
            if (!boost::filesystem::exists(file)) {
                remove = true;
            }
        }
        catch(...) {
        }

        if (remove) {
            stmtRemove.BindInt(0, allTracks.ColumnInt(0));
            stmtRemove.Step();
            stmtRemove.Reset();
        }
    }
}

//////////////////////////////////////////
///\brief
///Removes information related to removed tracks.
///
///This should be called after SyncDelete() to clean up the mess :)
///
///\see
///<SyncDelete>
//////////////////////////////////////////
void Indexer::SyncCleanup() {
    // Remove old artists
    this->dbConnection.Execute("DELETE FROM track_artists WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM artists WHERE id NOT IN (SELECT DISTINCT(visual_artist_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(album_artist_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(artist_id) FROM track_artists)");

    // Remove old genres
    this->dbConnection.Execute("DELETE FROM track_genres WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM genres WHERE id NOT IN (SELECT DISTINCT(visual_genre_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(genre_id) FROM track_genres)");

    // Remove old albums
    this->dbConnection.Execute("DELETE FROM albums WHERE id NOT IN (SELECT DISTINCT(album_id) FROM tracks)");

    // Remove metadata
    this->dbConnection.Execute("DELETE FROM track_meta WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM meta_values WHERE id NOT IN (SELECT DISTINCT(meta_value_id) FROM track_meta)");
    this->dbConnection.Execute("DELETE FROM meta_keys WHERE id NOT IN (SELECT DISTINCT(meta_key_id) FROM meta_values)");

    // ANALYZE
    this->dbConnection.Execute("ANALYZE");

    // Vacuum to remove unwanted space
    this->dbConnection.Execute("VACUUM");

    this->TrackRefreshed();
}

//////////////////////////////////////////
///\brief
///Get a vector with all sync paths
//////////////////////////////////////////
void Indexer::GetPaths(std::vector<std::string>& paths) {
    db::Connection connection;
    connection.Open(this->dbFilename.c_str());
    db::Statement stmt("SELECT path FROM paths ORDER BY id", connection);

    while (stmt.Step() == db::Row) {
        paths.push_back(stmt.ColumnText(0));
    }
}

static int optimize(
    musik::core::db::Connection &connection,
    std::string singular,
    std::string plural)
{
    std::string outer = boost::str(
        boost::format("SELECT id, lower(trim(name)) AS %1% FROM %2% ORDER BY %3%")
        % singular % plural % singular);

    db::Statement outerStmt(outer.c_str(), connection);

    std::string inner = boost::str(boost::format("UPDATE %1% SET sort_order=? WHERE id=?") % plural);
    db::Statement innerStmt(inner.c_str(), connection);

    int count = 0;
    while (outerStmt.Step() == db::Row) {
        innerStmt.BindInt(0, count);
        innerStmt.BindInt(1, outerStmt.ColumnInt(0));
        innerStmt.Step();
        innerStmt.Reset();
        ++count;
    }

    boost::thread::yield();

    return count;
}

//////////////////////////////////////////
///\brief
///Optimizes and fixing sorting and indexes
//////////////////////////////////////////
void Indexer::SyncOptimize() {
    DBID count = 0, id = 0;

    {
        db::ScopedTransaction transaction(this->dbConnection);
        optimize(this->dbConnection, "genre", "genres");
    }

    {
        db::ScopedTransaction transaction(this->dbConnection);
        optimize(this->dbConnection, "artist", "artists");
    }

    {
        db::ScopedTransaction transaction(this->dbConnection);
        optimize(this->dbConnection, "album", "albums");
    }

    {
        db::ScopedTransaction transaction(this->dbConnection);
        optimize(this->dbConnection, "content", "meta_values");
    }
}

//////////////////////////////////////////
///\brief
///Method for adding/removing paths in the database
//////////////////////////////////////////
void Indexer::ProcessAddRemoveQueue() {
    boost::mutex::scoped_lock lock(this->exitMutex);

    while (!this->addRemoveQueue.empty()) {
        AddRemoveContext context = this->addRemoveQueue.front();

        if (context.add) { /* insert new paths */
            db::Statement stmt("SELECT id FROM paths WHERE path=?", this->dbConnection);
            stmt.BindText(0, context.path);

            if (stmt.Step() != db::Row) {
                db::Statement insertPath("INSERT INTO paths (path) VALUES (?)", this->dbConnection);
                insertPath.BindText(0, context.path);
                insertPath.Step();
            }
        }
        else { /* remove old ones */
            db::Statement stmt("DELETE FROM paths WHERE path=?", this->dbConnection);
            stmt.BindText(0, context.path);
            stmt.Step();
        }

        this->addRemoveQueue.pop_front();
    }

    this->PathsUpdated();
}

void Indexer::RunAnalyzers() {
    typedef audio::IAnalyzer PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    typedef std::shared_ptr<PluginType> PluginPtr;
    typedef std::vector<PluginPtr> PluginVector;

    /* short circuit if there aren't any analyzers */

    PluginVector analyzers = PluginFactory::Instance()
        .QueryInterface<PluginType, Deleter>("GetAudioAnalyzer");

    if (analyzers.empty()) {
        return;
    }

    /* initialize status */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 6;
    }

    /* for each track... */

    DBID trackId = 0;

    db::Statement getNextTrack(
        "SELECT id FROM tracks WHERE id>? ORDER BY id LIMIT 1",
        this->dbConnection);

    getNextTrack.BindInt(0, trackId);

    while(getNextTrack.Step() == db::Row ) {
        trackId = getNextTrack.ColumnInt(0);

        getNextTrack.Reset();
        getNextTrack.UnBindAll();

        IndexerTrack track(trackId);

        if (LibraryTrack::Load(&track, this->dbConnection)) {
            PluginVector runningAnalyzers;

            PluginVector::iterator plugin = analyzers.begin();
            for ( ; plugin != analyzers.end(); ++plugin) {
                if ((*plugin)->Start(&track)) {
                    runningAnalyzers.push_back(*plugin);
                }
            }

            if(!runningAnalyzers.empty()) {
                audio::StreamPtr stream = audio::Stream::Create(audio::Stream::NoDSP);

                if (stream) {
                    if (stream->OpenStream(track.URI())) {

                        /* decode the stream quickly, passing to all analyzers*/

                        audio::BufferPtr buffer;

                        while ((buffer = stream->GetNextProcessedOutputBuffer()) && !runningAnalyzers.empty()) {
                            PluginVector::iterator plugin = runningAnalyzers.begin();
                            while(plugin != runningAnalyzers.end()) {
                                if ((*plugin)->Analyze(&track, buffer.get())) {
                                    ++plugin;
                                }
                                else {
                                    plugin = runningAnalyzers.erase(plugin);
                                }
                            }
                        }

                        /* done with track decoding and analysis, let the plugins know */
                        int successPlugins = 0;
                        PluginVector::iterator plugin = analyzers.begin();

                        for( ; plugin != analyzers.end(); ++plugin){
                            if((*plugin)->End(&track)){
                                successPlugins++;
                            }
                        }

                        /* the analyzers can write metadata back to the DB, so if any of them
                        completed successfully, then save the track. */

                        if(successPlugins>0) {
                            track.Save(this->dbConnection, this->libraryPath);
                        }
                    }
                }
            }
        }

        if (this->Exited() || this->Restarted()){
            return;
        }

        getNextTrack.BindInt(0, trackId);
    }
}
