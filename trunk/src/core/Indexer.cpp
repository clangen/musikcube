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
#include <core/Track.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>

#include <boost/filesystem.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>

using namespace musik::core;

Indexer::Indexer(void) : oThread(NULL),iTimeout(3600),iProgress(0),iStatus(0){
}

Indexer::~Indexer(void){
    if(this->oThread){
        this->Exit(true);
        this->oThread->join();
        delete this->oThread;
        this->oThread    = NULL;
    }
}
int Indexer::GetStatusCode(){
    boost::mutex::scoped_lock oLock(this->oProgressMutex);
    return this->iStatus;
}

utfstring Indexer::GetStatus(){
    boost::mutex::scoped_lock oLock(this->oProgressMutex);
    utfstring sStatus;
    switch(this->iStatus){
        case 1:
            sStatus    = boost::str( boost::wformat(UTF("Counting files: %1%"))%this->iNOFFiles );
//            sStatus.Format(UTF("Counting files: %d"),this->iNOFFiles);
            break;
        case 2:
            sStatus    = boost::str( boost::wformat(UTF("Indexing: %1%"))%(this->iProgress*100));
            break;
        case 3:
            sStatus    = boost::str( boost::wformat(UTF("Cleaning up: %1%"))%(this->iProgress*100));
            break;
    }
    return sStatus;
}


void Indexer::RestartSync(bool bNewRestart){
    boost::mutex::scoped_lock oLock(this->exitMutex);
    this->bRestart    = bNewRestart;
    if(this->bRestart){
        this->Notify();
    }
}

bool Indexer::Restarted(){
    boost::mutex::scoped_lock oLock(this->exitMutex);
    return this->bRestart;
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
void Indexer::AddPath(utfstring sPath){
    boost::filesystem::wpath oPath(sPath);
    sPath    = oPath.string();    // Fix pathname for slash/backslash
    if(sPath.substr(sPath.size()-1,1)!=UTF("/")){
        sPath    += UTF("/");
    }

    Indexer::_AddRemovePath addPath;
    addPath.add         = true;
    addPath.path        = sPath;

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(addPath);
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
void Indexer::RemovePath(utfstring sPath){

    Indexer::_AddRemovePath removePath;
    removePath.add          = false;
    removePath.path         = sPath;

    {
        boost::mutex::scoped_lock lock(this->exitMutex);
        this->addRemoveQueue.push_back(removePath);
    }

    this->RestartSync();
}


//////////////////////////////////////////
///\brief
///Main method for doing the synchronization.
//////////////////////////////////////////
void Indexer::Synchronize(){

    typedef Plugin::IMetaDataReader PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    //
    this->metadataReaders =
        PluginFactory::Instance().QueryInterface<PluginType, Deleter>("GetMetaDataReader");

    {
        boost::mutex::scoped_lock oLock(this->oProgressMutex);
        this->iNOFFiles        = 0;
        this->iFilesIndexed    = 0;
    }

    this->SyncAddRemovePaths();

    // Get the paths
    std::vector<utfstring> aPaths;
    std::vector<DBINT> aPathIds;

    {
        db::Statement stmt("SELECT id,path FROM paths",this->dbConnection);

        while( stmt.Step()==db::ReturnCode::Row ){
            // For each path
            DBINT iPathId( stmt.ColumnInt(0) );
            utfstring sPath( stmt.ColumnTextUTF(1) );

            // Check to see if folder exists, otherwise ignore (unavaliable paths are not removed)
            boost::filesystem::utfpath oFolder(sPath);
            try{
                if(boost::filesystem::exists(oFolder)){
                    aPaths.push_back(sPath);
                    aPathIds.push_back(iPathId);
                }
            }
            catch(...){
            }
        }
    }

    // Count files
    {
        boost::mutex::scoped_lock oLock(this->oProgressMutex);
        this->iStatus    = 1;
    }

    for(int i(0);i<aPaths.size();++i){
        utfstring sPath    = aPaths[i];
        this->CountFiles(sPath);
    }

    // Index files
    {
        boost::mutex::scoped_lock oLock(this->oProgressMutex);
        this->iStatus    = 2;
    }

    for(int i(0);i<aPaths.size();++i){
        utfstring sPath    = aPaths[i];
        DBINT iPathId    = aPathIds[i];

        this->SyncDirectory(sPath,0,iPathId);
    }

    {
        boost::mutex::scoped_lock oLock(this->oProgressMutex);
        this->iProgress    = 1.0;
        this->iStatus    = 3;
    }

    // Cleaning up
    if(!this->Restarted() && !this->Exit()){
        this->SyncDelete(aPathIds);
    }
    if(!this->Restarted() && !this->Exit()){
        this->SyncCleanup();
    }
    if(!this->Restarted() && !this->Exit()){
        this->SyncOptimize();
    }

    // Unload the IMetaDataReaders
    this->metadataReaders.clear();

    {
        boost::mutex::scoped_lock oLock(this->oProgressMutex);
        this->iStatus    = 0;
    }
}

//////////////////////////////////////////
///\brief
///Counts the number of files to synchronize
///
///\param sFolder
///Folder to count files in.
//////////////////////////////////////////
void Indexer::CountFiles(utfstring &sFolder){
    if(!this->Exit() && !this->Restarted()){
        boost::filesystem::utfpath oPath(sFolder);
        try{
            boost::filesystem::utfdirectory_iterator oEndFile;
            for(boost::filesystem::utfdirectory_iterator oFile(oPath);oFile!=oEndFile;++oFile){
                if(is_directory(oFile->status())){
                    utfstring sDirectory;
                    sDirectory.assign(oFile->path().string());
                    this->CountFiles(sDirectory);
                }else{
                    boost::mutex::scoped_lock oLock(this->oProgressMutex);
                    this->iNOFFiles++;
                }
            }
        }
        catch(...){

        }
    }

}

//////////////////////////////////////////
///\brief
///Reads all tracks in a folder
///
///\param sFolder
///Folder to check files in
///
///\param iParentFolderId
///Database ID of the parent folder (folders table)
///
///\param iPathId
///Database ID of the current path (paths table)
///
///Read all tracks in a folder. All folders in that folder is recursively called.
//////////////////////////////////////////
void Indexer::SyncDirectory(utfstring &sFolder,DBINT iParentFolderId,DBINT iPathId){

    if(this->Exit() || this->Restarted()){
        return;
    }

    boost::filesystem::utfpath oPath(sFolder);
    sFolder    = oPath.string();    // Fix pathname for slash/backslash

    utfstring sFolderLeaf(oPath.leaf());
    DBINT iFolderId(0);

    // Get this folder ID
    {
        db::CachedStatement stmt("SELECT id FROM folders WHERE name=? AND path_id=? AND parent_id=?",this->dbConnection);
        stmt.BindTextUTF(0,sFolderLeaf);
        stmt.BindInt(1,iPathId);
        stmt.BindInt(2,iParentFolderId);

        if(stmt.Step()==db::ReturnCode::Row){
            iFolderId    = stmt.ColumnInt(0);
        }
    }

    boost::thread::yield();

    // If it do not exists, create it
    if(iFolderId==0){

        db::CachedStatement stmt("INSERT INTO folders (name,path_id,parent_id,fullpath) VALUES(?,?,?,?)",this->dbConnection);
        stmt.BindTextUTF(0,sFolderLeaf);
        stmt.BindInt(1,iPathId);
        stmt.BindInt(2,iParentFolderId);
        stmt.BindTextUTF(3,sFolder);

        if(stmt.Step()==db::ReturnCode::Done){
            iFolderId    = this->dbConnection.LastInsertedId();
        }else{
            // Error, failed to insert
            return;
        }

    }


    boost::thread::yield();

    // Loop through the files in the current folder

    try{        // boost::filesystem may throw
        boost::filesystem::utfdirectory_iterator oEndFile;
        for(boost::filesystem::utfdirectory_iterator oFile(oPath);oFile!=oEndFile && !this->Exit() && !this->Restarted();++oFile){

            if(is_directory(oFile->status())){

                // It's a directory
                utfstring oDirectory;
                oDirectory.assign(oFile->path().string());

                // If this is a directory, recurse down
                this->SyncDirectory(oDirectory,iFolderId,iPathId);

            }else{

                ++this->iFilesIndexed;            // Count the files indexed.
                if(this->iFilesIndexed%5==0){    // Every 5s file
                    boost::mutex::scoped_lock oLock(this->oProgressMutex);
                    this->iProgress    = (double)this->iFilesIndexed/(double)this->iNOFFiles;
                }
                // This is a file, create a IndexerTrack object
                //IndexerTrack oTrack(oFile->path());
                musik::core::Track track;
                track.InitMeta(NULL);   // Not threadsafe, only used in this thread.

                // Get file-info from database
                if(track.CompareDBAndFileInfo(oFile->path(),this->dbConnection,iFolderId)){

                    // Find out what TagReader can read the file
                    bool tagRead=false;
                    typedef MetadataReaderList::iterator Iterator;
                    Iterator it = this->metadataReaders.begin();
                    while (it != this->metadataReaders.end()) {
                        if((*it)->CanReadTag(track.GetValue("extension")) ){
                            // Should be able to read the tag
                            if( (*it)->ReadTag(&track) ){
                                // Successfully read the tag.
                                tagRead=true;
                            }
                        }

                        it++;
                    }

                    if(tagRead){
                        // Save
                        track.Save(this->dbConnection,this->libraryPath,iFolderId);
                    }

                }

            }

            boost::thread::yield();

        }
    }
    catch(...){
//        std::wcout << UTF("ERROR ") << sFolder << std::endl;
    }
}

//////////////////////////////////////////
///\brief
///Main loop the thread is running in.
//////////////////////////////////////////
void Indexer::ThreadLoop(){


    while(!this->Exit()){

        // Database should only be open when synchronizing
        this->dbConnection.Open(this->database.c_str(),0,4096);

        this->RestartSync(false);
        this->Synchronize();

        this->dbConnection.Close();


        boost::xtime oWaitTime;
        boost::xtime_get(&oWaitTime, boost::TIME_UTC);
        oWaitTime.sec += this->iTimeout;

        if(!this->Restarted()){
            this->NotificationTimedWait(oWaitTime);
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
bool Indexer::Startup(utfstring setLibraryPath){

    this->libraryPath    = setLibraryPath;

    // Create thumbnail cache directory
    boost::filesystem::utfpath thumbPath(this->libraryPath+UTF("thumbs/"));
    if(!boost::filesystem::exists(thumbPath)){
        boost::filesystem::create_directories(thumbPath);
    }

    // start the thread
    this->oThread        = new boost::thread(boost::bind(&Indexer::ThreadLoop,this));
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
void Indexer::SyncDelete(std::vector<DBINT> aPaths){

    // Remove unwanted folder-paths
    this->dbConnection.Execute("DELETE FROM folders WHERE path_id NOT IN (SELECT id FROM paths)");

    {
        // Remove non existing folders
        db::Statement stmt("SELECT id,fullpath FROM folders WHERE path_id=?",this->dbConnection);
        db::Statement stmtRemove("DELETE FROM folders WHERE id=?",this->dbConnection);

        for(int i(0);i<aPaths.size();++i){

            stmt.BindInt(0,aPaths[i]);

            while( stmt.Step()==db::ReturnCode::Row && !this->Exit() && !this->Restarted() ){
                // Check to see if file still exists
                bool bRemove(true);
                utfstring sFolder   = stmt.ColumnTextUTF(1);

                try{
                    boost::filesystem::utfpath oFolder(sFolder);
                    if(boost::filesystem::exists(oFolder)){
                        bRemove    = false;
                    }
                }
                catch(...){
                }

                if(bRemove){
                    // Remove the folder
                    stmtRemove.BindInt(1,stmt.ColumnInt(0));
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
    DBINT iSongs(0),iCount(0);
    if(stmtCount.Step()==db::ReturnCode::Row){
        iSongs = stmtCount.ColumnInt(0);
    }


    db::Statement stmt("SELECT t.id,f.fullpath||'/'||t.filename FROM tracks t,folders f WHERE t.folder_id=f.id AND f.path_id=?",this->dbConnection);
    db::Statement stmtRemove("DELETE FROM tracks WHERE id=?",this->dbConnection);

    for(int i(0);i<aPaths.size();++i){
        stmt.BindInt(0,aPaths[i]);

        while( stmt.Step()==db::ReturnCode::Row  && !this->Exit() && !this->Restarted() ){
            // Check to see if file still exists
            {
                boost::mutex::scoped_lock oLock(this->oProgressMutex);
                if(iSongs>0){
                    this->iProgress    = 0.2+0.8*(double)iCount/(double)iSongs;
                }
            }

            bool bRemove(true);
            utfstring sFile = stmt.ColumnTextUTF(1);

            try{
                boost::filesystem::utfpath oFile(sFile);
                if(boost::filesystem::exists(oFile)){
                    bRemove    = false;
                }
            }
            catch(...){
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

}

std::vector<utfstring> Indexer::GetPaths(){
    std::vector<utfstring> aPaths;

    db::Statement stmt("SELECT path FROM paths ORDER BY id",this->dbConnection);

    while(stmt.Step()==db::ReturnCode::Row){
        aPaths.push_back(stmt.ColumnTextUTF(0));
    }

    return aPaths;
}

//////////////////////////////////////////
///\brief
///Optimizes and fixing sorting and indexes
//////////////////////////////////////////
void Indexer::SyncOptimize(){

    DBINT iCount(0),iId(0);


    {
        db::ScopedTransaction transaction(this->dbConnection);

        // Fix sort order on genres
        db::Statement stmt("SELECT id,lower(trim(name)) AS genre FROM genres ORDER BY genre",this->dbConnection);
        db::Statement stmtUpdate("UPDATE genres SET sort_order=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::ReturnCode::Row){

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
        while(stmt.Step()==db::ReturnCode::Row){

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
        while(stmt.Step()==db::ReturnCode::Row){

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

        db::Statement stmt("SELECT t.id FROM tracks t LEFT OUTER JOIN genres g ON g.id=t.visual_genre_id LEFT OUTER JOIN artists ar ON ar.id=t.visual_artist_id LEFT OUTER JOIN albums al ON al.id=t.album_id LEFT OUTER JOIN folders f ON f.id=t.folder_id ORDER BY g.sort_order,ar.sort_order,al.sort_order,t.track,f.fullpath,t.filename",this->dbConnection);

        db::Statement stmtUpdate("UPDATE tracks SET sort_order1=? WHERE id=?",this->dbConnection);
        iCount    = 0;
        while(stmt.Step()==db::ReturnCode::Row){

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


void Indexer::SyncAddRemovePaths(){

    boost::mutex::scoped_lock lock(this->exitMutex);

    // Loop through all add-remove paths
    while(!this->addRemoveQueue.empty()){
        if(this->addRemoveQueue.front().add){
            // Add front path

            // First check for the path
            db::Statement stmt("SELECT id FROM paths WHERE path=?",this->dbConnection);
            stmt.BindTextUTF(0,this->addRemoveQueue.front().path);

            if(stmt.Step()==db::ReturnCode::Row){
                // Path already exists. Do not add
            }else{
                db::Statement insertPath("INSERT INTO paths (path) VALUES (?)",this->dbConnection);
                insertPath.BindTextUTF(0,this->addRemoveQueue.front().path);
                insertPath.Step();
            }

        }else{
            // Remove front path
            db::Statement stmt("DELETE FROM paths WHERE path=?",this->dbConnection);
            stmt.BindTextUTF(0,this->addRemoveQueue.front().path);
            stmt.Step();
        }

        // remove the front and go to the next.
        this->addRemoveQueue.pop_front();

    }

    // Small cleanup
    this->dbConnection.Execute("DELETE FROM folders WHERE path_id NOT IN (SELECT id FROM paths)");

}

int Indexer::LongRunningQueryInterrupt(void *indexer){

    Indexer *thisIndexer    = (Indexer*)indexer;

    if(thisIndexer->Exit()){
        return 1;
    }else{
        return 0;
    }
}



