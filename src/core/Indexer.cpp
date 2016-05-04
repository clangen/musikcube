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

#include <core/Indexer.h>

#include <core/config.h>
#include <core/config.h>
#include <core/IndexerTrack.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/PluginFactory.h>
#include <core/Preferences.h>
#include <core/sdk/IAnalyzer.h>
#include <core/audio/Stream.h>

#include <boost/thread/xtime.hpp>
#include <boost/bind.hpp>

using namespace musik::core;

Indexer::Indexer() 
: thread(NULL)
, progress(0)
, progress2(0)
, status(0)
, restart(false)
, nofFiles(0)
, filesIndexed(0)
, filesSaved(0)
{
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
///Get the current status (text)
//////////////////////////////////////////
std::string Indexer::GetStatus() {
    boost::mutex::scoped_lock lock(this->progressMutex);
    
    std::string status;
    switch(this->status) {
        case 1: return boost::str(boost::format("Counting files: %1%")%this->nofFiles );
        case 2: return boost::str(boost::format("Indexing: %.2f") % (this->progress*100)) + "%";
        case 3: return boost::str(boost::format("Removing old files: %.2f") % (this->progress*100)) + "%";
        case 4: return "Cleaning up.";
        case 5: return "Optimizing.";
        case 6: return boost::str(boost::format("Analyzing: %.2f%% (current %.1f%%)")
            % (100.0 * this->progress / (double) this->nofFiles)
            % (this->progress2*100.0));
    }

    return status;
}


//////////////////////////////////////////
///\brief
///Restart the sync
///
///\param bNewRestart
///Should if be restarted or not
//////////////////////////////////////////
void Indexer::RestartSync(bool restart){
    boost::mutex::scoped_lock lock(this->exitMutex);

    this->restart = restart;

    if (this->restart) {
        this->Notify(); /* rename Notify()? */
    }
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
void Indexer::AddPath(std::string path) {
    boost::filesystem::path fsPath(path);
    path = fsPath.string(); /* canonicalize */

    if (path.substr(path.size() -1 , 1) != "/") {
        path += "/";
    }

    Indexer::AddRemoveContext context;
    context.add = true;
    context.path = path;

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(context);
    }

    this->RestartSync();
}

//////////////////////////////////////////
///\brief
///Remove a path from the Indexer
///
///\param sPath
///Path to remove
//////////////////////////////////////////
void Indexer::RemovePath(std::string path) {

    Indexer::AddRemoveContext context;
    context.add = false;
    context.path = path;

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(context);
    }

    this->RestartSync();
}

//////////////////////////////////////////
///\brief
///Main method for doing the synchronization.
//////////////////////////////////////////
void Indexer::Synchronize() {
    /* load all of the metadata (tag) reader plugins */
    typedef Plugin::IMetaDataReader PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;

    this->metadataReaders = PluginFactory::Instance()
        .QueryInterface<PluginType, Deleter>("GetMetaDataReader");

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->nofFiles = 0;
        this->filesIndexed = 0;
    }

    this->ProcessAddRemoveQueue();

    /* get sync paths and ids from the db */

    std::vector<std::string> paths;
    std::vector<DBID> pathIds;

    {
        db::Statement stmt("SELECT id, path FROM paths", this->dbConnection);

        while (stmt.Step()==db::Row) {
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

    /* count the files that need to be processed */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 1;
        this->progress = 0.0;
    }

	for(std::size_t i = 0; i < paths.size(); ++i){
        std::string path = paths[i];
        this->CountFiles(path);
    }

    /* add new files  */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 2;
        this->progress = 0.0;
        this->filesSaved = 0;
    }
    
    for(std::size_t i = 0; i < paths.size(); ++i) {
        std::string path = paths[i];
        this->SyncDirectory(path ,0, pathIds[i], path);
    }

    /* remove undesired entries from db (files themselves will remain) */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->status = 3;
        this->progress = 0.0;
    }

    if (!this->Restarted() && !this->Exited()) {
        this->SyncDelete(pathIds);
    }

    /* cleanup -- remove stale artists, albums, genres, etc */

    {
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->progress = 0.0;
        this->status = 4;
    }


    if(!this->Restarted() && !this->Exited()){
        this->SyncCleanup();
    }

    /* optimize */

    {
       boost::mutex::scoped_lock lock(this->progressMutex);
        this->progress = 0.0;
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
}

//////////////////////////////////////////
///\brief
///Counts the number of files to synchronize
///
///\param dir
///Folder to count files in.
//////////////////////////////////////////
void Indexer::CountFiles(const std::string &dir) {
    if(!this->Exited() && !this->Restarted()){
        boost::filesystem::path path(dir);
        try{
            boost::filesystem::directory_iterator invalidFile;
            boost::filesystem::directory_iterator file(path);

            for ( ; file != invalidFile; ++file) {
                if (is_directory(file->status())) {
                    this->CountFiles(file->path().string());
                }
                else {
                    boost::mutex::scoped_lock lock(this->progressMutex);
                    this->nofFiles++;
                }
            }
        }
        catch(...) {
        }
    }

}

//////////////////////////////////////////
///\brief
///Reads all tracks in a folder
///
///\param dir
///Folder to check files in
///
///\param parentDirId
///Database ID of the parent folder (folders table)
///
///\param pathId
///Database ID of the current path (paths table)
///
///Read all tracks in a folder. All folders in that folder is recursively called.
//////////////////////////////////////////

void Indexer::SyncDirectory(
    const std::string &inDir,
    DBID parentDirId,
    DBID pathId, 
    std::string &syncPath)
{
    if(this->Exited() || this->Restarted()) {
        return;
    }

    boost::filesystem::path path(inDir);
    std::string dir = path.string(); /* canonicalize slash*/

    std::string leaf(path.leaf().string());
    DBID dirId = 0;

    /* get folder id */

    {
        db::CachedStatement stmt("SELECT id FROM folders WHERE name=? AND path_id=? AND parent_id=?", this->dbConnection);
        stmt.BindText(0, leaf);
        stmt.BindInt(1, pathId);
        stmt.BindInt(2, parentDirId);

        if(stmt.Step()==db::Row) {
            dirId = stmt.ColumnInt(0);
        }
    }

    boost::thread::yield();

    /* no ID yet? needs to be inserted... */

    if (dirId == 0) {
        std::string relativePath(dir.substr(syncPath.size()));

        db::CachedStatement stmt("INSERT INTO folders (name, path_id, parent_id, relative_path) VALUES(?,?,?,?)", this->dbConnection);
        stmt.BindText(0, leaf);
        stmt.BindInt(1, pathId);
        stmt.BindInt(2, parentDirId);
        stmt.BindText(3, relativePath);

        if (!stmt.Step() == db::Done) {
            return; /* ugh, failed? */
        }

        dirId = this->dbConnection.LastInsertedId();
    }

    boost::thread::yield(); /* whistle... */

    /* start recursive filesystem scan */

    try { /* boost::filesystem may throw */
        boost::filesystem::directory_iterator end;
        boost::filesystem::directory_iterator file(path);
        for( ; file != end && !this->Exited() && !this->Restarted();++file) {
            if (is_directory(file->status())) {
                /* recursion here */
                this->SyncDirectory(file->path().string(), dirId,pathId,syncPath);
            }
            else {
                ++this->filesIndexed;

                /* update progress every so often... */

                if (this->filesIndexed % 25 == 0) {
                    boost::mutex::scoped_lock lock(this->progressMutex);
                    this->progress = (double) this->filesIndexed / (double)this->nofFiles;
                }

                musik::core::IndexerTrack track(0);

                /* get cached filesize, parts, size, etc */
                if(track.CompareDBAndFileInfo(file->path(),this->dbConnection,dirId)){
                    bool saveToDb = false;

                    /* read the tag from the plugin */

                    typedef MetadataReaderList::iterator Iterator;
                    Iterator it = this->metadataReaders.begin();
                    while (it != this->metadataReaders.end()) {
                        if ((*it)->CanReadTag(track.GetValue("extension").c_str())) {
                            if ((*it)->ReadTag(file->path().string().c_str(), &track)) {
                                saveToDb = true;
                            }
                        }

                        it++;
                    }

                    /* write it to the db, if we read successfully*/

                    if (saveToDb) {
                        track.Save(this->dbConnection, this->libraryPath, dirId);

                        this->filesSaved++;
                        if (this->filesSaved % 100 == 0) {
                            this->TrackRefreshed(); /* no idea... */
                        }
                    }
                }
            }

            boost::thread::yield();
        }
    }
    catch(...){
    }
}

//////////////////////////////////////////
///\brief
///Main loop the thread is running in.
//////////////////////////////////////////
void Indexer::ThreadLoop(){

    bool firstTime(true);

    while(!this->Exited()){

        // Get preferences
        Preferences prefs("Indexer");

        if(!firstTime || (firstTime && prefs.GetBool("SyncOnStartup",true))){

            this->SynchronizeStart();

            // Database should only be open when synchronizing
            this->dbConnection.Open(this->database.c_str(),0);
            this->RestartSync(false);
            this->Synchronize();
            this->RunAnalyzers();
            {
                boost::mutex::scoped_lock lock(this->progressMutex);
                this->status       = 0;
            }
            this->dbConnection.Close();

            this->SynchronizeEnd();
        }
        firstTime   = false;

        
        int syncTimeout = prefs.GetInt("SyncTimeout",3600);

        if(syncTimeout){
            // Sync every "syncTimeout" second
            boost::xtime oWaitTime;
            boost::xtime_get(&oWaitTime, boost::TIME_UTC_);
            oWaitTime.sec += syncTimeout;

            if(!this->Restarted()){
                this->NotificationTimedWait(oWaitTime);
            }
        }else{
            // No continous syncing
            if(!this->Restarted()){
                this->NotificationWait();
            }
        }

    }

}

//////////////////////////////////////////
///\brief
///Start up the Indexer.
///
///\returns
///true if successfully started.
///
///This method will start the Indexer thread that will
///start up ThreadLoop and then return.
///
///\see
///<ThreadLoop>
//////////////////////////////////////////
bool Indexer::Startup(std::string setLibraryPath){

    this->libraryPath    = setLibraryPath;

    // Create thumbnail cache directory
    boost::filesystem::path thumbPath(this->libraryPath+"thumbs/");
    if(!boost::filesystem::exists(thumbPath)){
        boost::filesystem::create_directories(thumbPath);
    }

    // start the thread
    try{
        this->thread        = new boost::thread(boost::bind(&Indexer::ThreadLoop,this));
    }
    catch(...){
        return false;
    }
    return true;
}

//////////////////////////////////////////
///\brief
///Part of the synchronizer to delete removed files.
///
///\param aPaths
///Vector of path-id to check for deletion
///
///This method will first check for removed folders and automaticaly
///delete all tracks in the removed folders.
///Secondly it will check all files it they are removed.
///
///\remarks
///This method will not delete related information (meta-data, albums, etc)
//////////////////////////////////////////
void Indexer::SyncDelete(const std::vector<DBID>& aPaths){

    // Remove unwanted folder-paths
    this->dbConnection.Execute("DELETE FROM folders WHERE path_id NOT IN (SELECT id FROM paths)");

    db::Statement stmtSyncPath("SELECT p.path FROM paths p WHERE p.id=?",this->dbConnection);

    {
        // Remove non existing folders
        db::Statement stmt("SELECT f.id,p.path||f.relative_path FROM folders f,paths p WHERE f.path_id=p.id AND p.id=?",this->dbConnection);
        db::Statement stmtRemove("DELETE FROM folders WHERE id=?",this->dbConnection);

        for(std::size_t i(0);i<aPaths.size();++i){

            stmt.BindInt(0,aPaths[i]);

            // Get the syncpath
            stmtSyncPath.BindInt(0,aPaths[i]);
            stmtSyncPath.Step();
            std::string syncPathString( stmtSyncPath.ColumnText(0) );
            stmtSyncPath.Reset();

            while( stmt.Step()==db::Row && !this->Exited() && !this->Restarted() ){
                // Check to see if file still exists

                bool bRemove(true);
                std::string dir   = stmt.ColumnText(1);

                try{
                    boost::filesystem::path oFolder(dir);
                    if(boost::filesystem::exists(oFolder)){
                        bRemove    = false;
                    }else{
                        // Also do a check so that the syncpath is still up, or else do not remove
                        boost::filesystem::path syncPath(syncPathString);
                        if(!boost::filesystem::exists(syncPath)){
                            bRemove    = false;
                        }
                    }
                }
                catch(...){
                    bRemove    = false;
                }

                if(bRemove){
                    // Remove the folder
                    stmtRemove.BindInt(0,stmt.ColumnInt(0));
                    stmtRemove.Step();
                    stmtRemove.Reset();
                }
            }

            stmt.Reset();
        }
    }


    boost::thread::yield();

    // Remove songs with no folder
    this->dbConnection.Execute("DELETE FROM tracks WHERE folder_id NOT IN (SELECT id FROM folders)");

    boost::thread::yield();

    // Remove tracks
    db::Statement stmtCount("SELECT count(*) FROM tracks",this->dbConnection);
    DBID iSongs(0),iCount(0);
    if(stmtCount.Step()==db::Row){
        iSongs = stmtCount.ColumnInt(0);
    }


    db::Statement stmt("SELECT t.id,p.path||f.relative_path||'/'||t.filename FROM tracks t,folders f,paths p WHERE t.folder_id=f.id AND f.path_id=p.id AND p.id=?",this->dbConnection);
    db::Statement stmtRemove("DELETE FROM tracks WHERE id=?",this->dbConnection);

    for(std::size_t i(0);i<aPaths.size();++i){
        stmt.BindInt(0,aPaths[i]);

        // Get the syncpath
        stmtSyncPath.BindInt(0,aPaths[i]);
        stmtSyncPath.Step();
        std::string syncPathString( stmtSyncPath.ColumnText(0) );
        stmtSyncPath.Reset();

        while( stmt.Step()==db::Row  && !this->Exited() && !this->Restarted() ){
            // Check to see if file still exists
            {
                boost::mutex::scoped_lock lock(this->progressMutex);
                if(iSongs>0){
                    this->progress    = 0.2+0.8*(double)iCount/(double)iSongs;
                }
            }

            bool bRemove(true);
            std::string sFile = stmt.ColumnText(1);

            try{
                boost::filesystem::path file(sFile);
                if(boost::filesystem::exists(file)){
                    bRemove    = false;
                }else{
                    // Also do a check so that the syncpath is still up, or else do not remove
                    boost::filesystem::path syncPath(syncPathString);
                    if(!boost::filesystem::exists(syncPath)){
                        bRemove    = false;
                    }
                }
            }
            catch(...){
                bRemove    = false;
            }

            if(bRemove){
                stmtRemove.BindInt(0,stmt.ColumnInt(0));
                stmtRemove.Step();
                stmtRemove.Reset();
            }
            ++iCount;
        }
        
        stmt.Reset();

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
void Indexer::SyncCleanup(){

    // Remove old artists
    this->dbConnection.Execute("DELETE FROM track_artists WHERE track_id NOT IN (SELECT id FROM tracks)");
    boost::thread::yield();
    this->dbConnection.Execute("DELETE FROM artists WHERE id NOT IN (SELECT DISTINCT(visual_artist_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(artist_id) FROM track_artists)");
    boost::thread::yield();

    // Remove old genres
    this->dbConnection.Execute("DELETE FROM track_genres WHERE track_id NOT IN (SELECT id FROM tracks)");
    boost::thread::yield();
    this->dbConnection.Execute("DELETE FROM genres WHERE id NOT IN (SELECT DISTINCT(visual_genre_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(genre_id) FROM track_genres)");
    boost::thread::yield();

    // Remove old albums
    this->dbConnection.Execute("DELETE FROM albums WHERE id NOT IN (SELECT DISTINCT(album_id) FROM tracks)");
    boost::thread::yield();

    // Remove metadata
    this->dbConnection.Execute("DELETE FROM track_meta WHERE track_id NOT IN (SELECT id FROM tracks)");
    boost::thread::yield();
    this->dbConnection.Execute("DELETE FROM meta_values WHERE id NOT IN (SELECT DISTINCT(meta_value_id) FROM track_meta)");
    boost::thread::yield();
    this->dbConnection.Execute("DELETE FROM meta_keys WHERE id NOT IN (SELECT DISTINCT(meta_key_id) FROM meta_values)");
    boost::thread::yield();

    // ANALYZE
    this->dbConnection.Execute("ANALYZE");
    boost::thread::yield();

    // Vacuum to remove unwanted space
    this->dbConnection.Execute("VACUUM");

    this->TrackRefreshed();

}

//////////////////////////////////////////
///\brief
///Get a vector with all sync paths
//////////////////////////////////////////
std::vector<std::string> Indexer::GetPaths(){
    std::vector<std::string> aPaths;

    db::Connection tempDB;

    tempDB.Open(this->database.c_str());

    db::Statement stmt("SELECT path FROM paths ORDER BY id",tempDB);

    while(stmt.Step()==db::Row){
        aPaths.push_back(stmt.ColumnText(0));
    }

    return aPaths;
}

//////////////////////////////////////////
///\brief
///Optimizes and fixing sorting and indexes
//////////////////////////////////////////
void Indexer::SyncOptimize(){

    DBID iCount(0),iId(0);


    {
        db::ScopedTransaction transaction(this->dbConnection);

        // Fix sort order on genres
        db::Statement stmt("SELECT id,lower(trim(name)) AS genre FROM genres ORDER BY genre",this->dbConnection);
        db::Statement stmtUpdate("UPDATE genres SET sort_order=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::Row){

            stmtUpdate.BindInt(0,iCount);
            stmtUpdate.BindInt(1,stmt.ColumnInt(0));
            stmtUpdate.Step();
            stmtUpdate.Reset();

            ++iCount;
        }
    }

    boost::thread::yield();


    {
        db::ScopedTransaction transaction(this->dbConnection);
        // Fix sort order on artists
        db::Statement stmt("SELECT id,lower(trim(name)) AS artist FROM artists ORDER BY artist",this->dbConnection);
        db::Statement stmtUpdate("UPDATE artists SET sort_order=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::Row){

            stmtUpdate.BindInt(0,iCount);
            stmtUpdate.BindInt(1,stmt.ColumnInt(0));
            stmtUpdate.Step();
            stmtUpdate.Reset();

            ++iCount;
        }
    }

    boost::thread::yield();


    {
        db::ScopedTransaction transaction(this->dbConnection);
        // Fix sort order on albums
        db::Statement stmt("SELECT id,lower(trim(name)) AS album FROM albums ORDER BY album",this->dbConnection);
        db::Statement stmtUpdate("UPDATE albums SET sort_order=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::Row){

            stmtUpdate.BindInt(0,iCount);
            stmtUpdate.BindInt(1,stmt.ColumnInt(0));
            stmtUpdate.Step();
            stmtUpdate.Reset();
            ++iCount;

        }
    }

    boost::thread::yield();


    {
        db::ScopedTransaction transaction(this->dbConnection);
        // Fix sort order on meta_values
        db::Statement stmt("SELECT id,lower(content) AS content FROM meta_values ORDER BY content",this->dbConnection);
        db::Statement stmtUpdate("UPDATE meta_values SET sort_order=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::Row){

            stmtUpdate.BindInt(0,iCount);
            stmtUpdate.BindInt(1,stmt.ColumnInt(0));
            stmtUpdate.Step();
            stmtUpdate.Reset();
            ++iCount;

        }
    }

    boost::thread::yield();


    {
        db::ScopedTransaction transaction(this->dbConnection);
        // Fix sort order on tracks
        /************************************
        The sort order of a track is by default in the following order
        genre, artist, album, track number, path, filename
        ************************************/

        db::Statement stmt("SELECT t.id FROM tracks t LEFT OUTER JOIN artists ar ON ar.id=t.visual_artist_id LEFT OUTER JOIN albums al ON al.id=t.album_id LEFT OUTER JOIN folders f ON f.id=t.folder_id ORDER BY ar.sort_order,al.sort_order,t.track,f.relative_path,t.filename",this->dbConnection);

        db::Statement stmtUpdate("UPDATE tracks SET sort_order1=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::Row){

            stmtUpdate.BindInt(0,iCount);
            stmtUpdate.BindInt(1,stmt.ColumnInt(0));
            stmtUpdate.Step();
            stmtUpdate.Reset();
            ++iCount;

            if(iCount%1000==0){
                transaction.CommitAndRestart();
            }
        }
    }


}


//////////////////////////////////////////
///\brief
///Method for adding/removing paths in the database
//////////////////////////////////////////
void Indexer::ProcessAddRemoveQueue(){

    boost::mutex::scoped_lock lock(this->exitMutex);

    // Loop through all add-remove paths
    while(!this->addRemoveQueue.empty()){
        if(this->addRemoveQueue.front().add){
            // Add front path

            // First check for the path
            db::Statement stmt("SELECT id FROM paths WHERE path=?",this->dbConnection);
            stmt.BindText(0,this->addRemoveQueue.front().path);

            if(stmt.Step()==db::Row){
                // Path already exists. Do not add
            }else{
                db::Statement insertPath("INSERT INTO paths (path) VALUES (?)",this->dbConnection);
                insertPath.BindText(0,this->addRemoveQueue.front().path);
                insertPath.Step();
            }

        }else{
            // Remove front path
            db::Statement stmt("DELETE FROM paths WHERE path=?",this->dbConnection);
            stmt.BindText(0,this->addRemoveQueue.front().path);
            stmt.Step();
        }

        // remove the front and go to the next.
        this->addRemoveQueue.pop_front();

    }

    // Small cleanup
    this->dbConnection.Execute("DELETE FROM folders WHERE path_id NOT IN (SELECT id FROM paths)");

    this->PathsUpdated();

}


void Indexer::RunAnalyzers(){
    typedef audio::IAnalyzer PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    typedef boost::shared_ptr<PluginType> PluginPtr;
    typedef std::vector<PluginPtr> PluginVector;

    // start by checking if there are any analyzers at all
    {
        PluginVector analyzers =
            PluginFactory::Instance().QueryInterface<PluginType, Deleter>("GetAudioAnalyzer");
        if(analyzers.empty()){
            return;
        }
    }

    {
        // Cleanup status
        boost::mutex::scoped_lock lock(this->progressMutex);
        this->progress     = 0;
        this->progress2    = 0;
        this->status       = 6;
    }

    // Get the number of tracks to be able to show percentage status.
    db::Statement totalTracks("SELECT count(*) FROM tracks",this->dbConnection);
    if(totalTracks.Step()==db::Row){
        this->nofFiles  = totalTracks.ColumnInt(0);
    }

    // Loop through all tracks
    DBID trackId(0);
    DBID folderId(0);
    db::Statement getNextTrack("SELECT id,folder_id FROM tracks WHERE id>? ORDER BY id LIMIT 1",this->dbConnection);
    getNextTrack.BindInt(0,trackId);

    while( getNextTrack.Step()==db::Row ){
        trackId     = getNextTrack.ColumnInt(0);
        folderId    = getNextTrack.ColumnInt(1);
        getNextTrack.Reset();
        getNextTrack.UnBindAll();

        boost::thread::yield();

        IndexerTrack track(trackId);
        if(track.GetTrackMetadata(this->dbConnection)){
            // lets see if we should analyze the track
            PluginVector analyzers =
                PluginFactory::Instance().QueryInterface<PluginType, Deleter>("GetAudioAnalyzer");

            // lets see what plugins that need to analyze the track audio
            for(PluginVector::iterator plugin=analyzers.begin();plugin!=analyzers.end();){
                if( (*plugin)->Start(&track) ){
                    ++plugin;
                }else{
                    plugin  = analyzers.erase(plugin);
                }
            }
            PluginVector runningAnalyzers(analyzers);

            if(!runningAnalyzers.empty()){
                // lets start a stream
                audio::StreamPtr stream = audio::Stream::Create(audio::Stream::NoDSP);
                if(stream){
                    if(stream->OpenStream(track.URL())){
                        // loop through the stream buffers, sending the buffers to the analyzers
                        audio::BufferPtr buffer;
                        while((buffer=stream->GetNextProcessedOutputBuffer()) && !runningAnalyzers.empty()){
                            {
                                boost::mutex::scoped_lock lock(this->progressMutex);
                                this->progress2 = stream->DecoderProgress();
                            }

                            PluginVector::iterator plugin=runningAnalyzers.begin();
                            while(plugin!=runningAnalyzers.end()){
                                // analyze the audio, and remove plugin if it returns false
                                if( (*plugin)->Analyze(&track,buffer.get()) ){
                                    ++plugin;
                                }else{
                                    plugin  = runningAnalyzers.erase(plugin);
                                }
                                boost::thread::yield();
                                if(this->Exited() || this->Restarted()){
                                    return;
                                }
                            }
                        }

                        // everything is analyzed, lets send the End
                        int successPlugins(0);
                        for(PluginVector::iterator plugin=analyzers.begin();plugin!=analyzers.end();++plugin){
                            if((*plugin)->End(&track)){
                                successPlugins++;
                            }
                        }

                        // finally save the track
                        if(successPlugins>0){
                            track.Save(this->dbConnection,this->libraryPath,folderId);
                        }
                        {
                            boost::mutex::scoped_lock lock(this->progressMutex);
                            this->progress2 = 0;
                        }
                    }
                }
            }

        }

        if(this->Exited() || this->Restarted()){
            return;
        }
        {
            boost::mutex::scoped_lock lock(this->progressMutex);
            this->progress++;
        }
        getNextTrack.BindInt(0,trackId);
    }

}

