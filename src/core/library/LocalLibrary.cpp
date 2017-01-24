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

#include <core/library/LocalLibrary.h>
#include <core/config.h>
#include <core/library/query/QueryBase.h>
#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/library/Indexer.h>
#include <core/runtime/Message.h>
#include <core/debug.h>

static const std::string TAG = "LocalLibrary";

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::runtime;

#define VERBOSE_LOGGING 0
#define MESSAGE_QUERY_COMPLETED 5000

class QueryCompletedMessage : public Message {
    public:
        QueryCompletedMessage(IMessageTarget* target, IQueryPtr query)
        : Message(target, MESSAGE_QUERY_COMPLETED, 0, 0) {
            this->query = query;
        }

        virtual ~QueryCompletedMessage() {
        }

        IQueryPtr GetQuery() { return this->query; }

    private:
        IQueryPtr query;
};

ILibraryPtr LocalLibrary::Create(std::string name, int id) {
    ILibraryPtr lib(new LocalLibrary(name, id));
    return lib;
}

LocalLibrary::LocalLibrary(std::string name,int id)
: name(name)
, id(id)
, exit(false)
, messageQueue(nullptr) {
    this->identifier = boost::lexical_cast<std::string>(id);

    auto prefs = Preferences::ForComponent("library");

    this->db.Open(
        this->GetDatabaseFilename().c_str(),
        0,
        prefs->GetInt("DatabaseCache",
        4096));

    LocalLibrary::CreateDatabase(this->db);

    this->indexer = new core::Indexer(
        this->GetLibraryDirectory(),
        this->GetDatabaseFilename());

    this->thread = new std::thread(std::bind(&LocalLibrary::ThreadProc, this));
}

LocalLibrary::~LocalLibrary() {
    this->Exit();
    this->thread->join();
    delete this->thread;
    delete this->indexer;
}

int LocalLibrary::Id() {
    return this->id;
}

const std::string& LocalLibrary::Name() {
    return this->name;
}

std::string LocalLibrary::GetLibraryDirectory() {
    std::string directory(musik::core::GetDataDirectory());

    if (!this->identifier.empty()) {
        directory.append(this->identifier + "/" );
    }

    boost::filesystem::path dir(directory);
    if(!boost::filesystem::exists(dir)){
        boost::filesystem::create_directories(dir);
    }

    directory = dir.string();

    return directory;
}

std::string LocalLibrary::GetDatabaseFilename() {
    return this->GetLibraryDirectory() + "musik.db";
}

int LocalLibrary::Enqueue(IQueryPtr query, unsigned int options) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (options & ILibrary::QuerySynchronous) {
        this->RunQuery(query, false); /* false = do not notify via QueryCompleted */
    }
    else {
        queryQueue.push_back(query);
        queueCondition.notify_all();

        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, "query '" + query->Name() + "' enqueued");
        }
    }

    return query->GetId();
}

bool LocalLibrary::Exited() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    return this->exit;
}

void LocalLibrary::Exit() {
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->exit = true;
    }

    /* kick sleeping threads back to the top of the loop */
    this->queueCondition.notify_all();
}

IQueryPtr LocalLibrary::GetNextQuery() {
    if (queryQueue.size()) {
        IQueryPtr front = queryQueue.front();
        queryQueue.pop_front();
        return front;
    }

    return IQueryPtr();
}

void LocalLibrary::RunQuery(IQueryPtr query, bool notify) {
    if (query) {
        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, "query '" + query->Name() + "' running");
        }

        query->Run(this->db);

        if (notify) {
            if (this->messageQueue) {
                this->messageQueue->Post(
                    std::shared_ptr<QueryCompletedMessage>(
                        new QueryCompletedMessage(this, query)));
            }
            else {
                this->QueryCompleted(query);
            }
        }

        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, boost::str(boost::format(
                "query '%1%' finished with status=%2%")
                % query->Name()
                % query->GetStatus()));
        }

        query.reset();
    }
}

void LocalLibrary::ThreadProc() {
    while (!this->Exited()) {
        IQueryPtr query;

        {
            std::unique_lock<std::recursive_mutex> lock(this->mutex);
            query = GetNextQuery();

            while (!query && !this->Exited()) {
                this->queueCondition.wait(lock);
                query = GetNextQuery();
            }
        }

        this->RunQuery(query);
    }
}

void LocalLibrary::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    this->messageQueue = &queue;
}

void LocalLibrary::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        this->QueryCompleted(static_cast<QueryCompletedMessage*>(&message)->GetQuery());
    }
}

musik::core::IIndexer* LocalLibrary::Indexer() {
    return this->indexer;
}

bool LocalLibrary::IsStaticMetaKey(std::string &metakey){
    static std::set<std::string> staticMetaKeys;

    if (staticMetaKeys.empty()) {
        staticMetaKeys.insert("track");
        staticMetaKeys.insert("disc");
        staticMetaKeys.insert("bpm");
        staticMetaKeys.insert("duration");
        staticMetaKeys.insert("filesize");
        staticMetaKeys.insert("year");
        staticMetaKeys.insert("title");
        staticMetaKeys.insert("filename");
        staticMetaKeys.insert("filetime");
    }

    return staticMetaKeys.find(metakey) != staticMetaKeys.end();
}

bool LocalLibrary::IsSpecialMTOMetaKey(std::string &metakey){
    static std::set<std::string> specialMTOMetaKeys;

    if (specialMTOMetaKeys.empty()) {
        specialMTOMetaKeys.insert("album");
        specialMTOMetaKeys.insert("visual_genre");
        specialMTOMetaKeys.insert("visual_artist");
    }

    return specialMTOMetaKeys.find(metakey)!=specialMTOMetaKeys.end();
}

bool LocalLibrary::IsSpecialMTMMetaKey(std::string &metakey) {
    static std::set<std::string> specialMTMMetaKeys;

    if (specialMTMMetaKeys.empty()) {
        specialMTMMetaKeys.insert("artist");
        specialMTMMetaKeys.insert("genre");
    }

    return specialMTMMetaKeys.find(metakey) != specialMTMMetaKeys.end();
}

void LocalLibrary::CreateDatabase(db::Connection &db){
    // Create the tracks-table
    db.Execute("CREATE TABLE IF NOT EXISTS tracks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "track INTEGER DEFAULT 0,"
        "disc TEXT DEFAULT '1',"
        "bpm REAL DEFAULT 0,"
        "duration INTEGER DEFAULT 0,"
        "filesize INTEGER DEFAULT 0,"
        "year INTEGER DEFAULT 0,"
        "visual_genre_id INTEGER DEFAULT 0,"
        "visual_artist_id INTEGER DEFAULT 0,"
        "album_artist_id INTEGER DEFAULT 0,"
        "path_id INTEGER,"
        "album_id INTEGER DEFAULT 0,"
        "title TEXT default '',"
        "filename TEXT default '',"
        "filetime INTEGER DEFAULT 0,"
        "thumbnail_id INTEGER DEFAULT 0)");

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

    // Create the thumbnails table
    db.Execute("CREATE TABLE IF NOT EXISTS thumbnails ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "filename TEXT default '',"
            "filesize INTEGER DEFAULT 0,"
            "checksum INTEGER DEFAULT 0"
            ")");

    // Create the playlists
    db.Execute("CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "user_id INTEGER default 0"
            ")");

    // Create the playlist_tracks table
    db.Execute("CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "track_id INTEGER DEFAULT 0,"
            "playlist_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0"
            ")");

    // INDEXES
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS users_index ON users (login)");
    db.Execute("CREATE UNIQUE INDEX IF NOT EXISTS paths_index ON paths (path)");
    db.Execute("CREATE INDEX IF NOT EXISTS genre_index ON genres (sort_order)");
    db.Execute("CREATE INDEX IF NOT EXISTS artist_index ON artists (sort_order)");
    db.Execute("CREATE INDEX IF NOT EXISTS album_index ON albums (sort_order)");
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
}
