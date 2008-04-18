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

#include <core/Track.h>
#include <core/Common.h>
#include <core/db/Statement.h>

#include <sqlite/sqlite3.h>
#include <boost/lexical_cast.hpp>

using namespace musik::core;

Track::Track(void) : meta(NULL),id(0){
}

Track::Track(DBINT newId) : meta(NULL),id(newId){
}

Track::~Track(void){
    this->ClearMeta();
}

//////////////////////////////////////////
///\brief
///Get value of a (one) tag
///
///\param key
///String identifier of the required value.
///
///\param threadHelper
///If you pass this ThreadHelper object, the GetValue will be threadsafe.
///
///\returns
///A const reference to a wstring (UTF-16) with the value. Empty string if not found.
//////////////////////////////////////////
/*const TrackMeta::Value& Track::GetValue(const TrackMeta::Key &key) const{

    if(this->meta){
        return this->meta->GetValue(key);
    }
    static utfstring emptyString;
    return emptyString;
}*/

const utfchar* Track::GetValue(const char* metakey) const{
    if(this->meta){
        return this->meta->GetValue(metakey);
    }
    return NULL;
}


//////////////////////////////////////////
///\brief
///Get all values of one tag identifier.
///
///\param metakey
///String identifier of the required value.
///
///\returns
///A pair of iterators. The first is the start iterator and the second is the end.
///
///\see
///GetValue
//////////////////////////////////////////
TrackMeta::TagMapIteratorPair Track::GetValues(const char* metakey) const{
    if(this->meta){
        return this->meta->GetValues(metakey);
    }
    return TrackMeta::TagMapIteratorPair();
}

TrackMeta::TagMapIteratorPair Track::GetAllValues() const{
    return TrackMeta::TagMapIteratorPair(this->meta->tags.begin(),this->meta->tags.end());
}


/*void Track::SetValue(const TrackMeta::Key &key,TrackMeta::Value &value){
    if(this->meta){
        this->meta->SetValue(key,value);
    }
}
*/
void Track::SetValue(const char* metakey,const utfchar* value){
    if(this->meta){
        this->meta->SetValue(metakey,value);
    }
}


void Track::ClearMeta(){
    if(this->meta){
        delete this->meta;
        this->meta    = NULL;
    }
}

void Track::InitMeta(Library::Base *library){
    if(!this->meta){
        this->meta    = new TrackMeta(library);
    }
}

bool Track::HasMeta(){
    return this->meta!=NULL;
}


bool Track::CompareDBAndFileInfo(const boost::filesystem::utfpath &file,db::Connection &dbConnection,DBINT currentFolderId){
 
    this->SetValue("path",file.string().c_str());
    this->SetValue("filename",file.leaf().c_str());

    int lastDot = file.leaf().find_last_of(UTF("."));
    if(lastDot!=utfstring::npos){
        this->SetValue("extension",file.leaf().substr(lastDot+1).c_str());
    }

    DBINT fileSize  = boost::filesystem::file_size(file);
    DBTIME fileTime = boost::filesystem::last_write_time(file);

    this->SetValue("filesize",boost::lexical_cast<utfstring>(fileSize).c_str());
    this->SetValue("filetime",boost::lexical_cast<utfstring>(fileTime).c_str());


    db::CachedStatement stmt("SELECT id,filename,filesize,filetime FROM tracks t WHERE folder_id=? AND filename=?",dbConnection);
    stmt.BindInt(0,currentFolderId);
    stmt.BindTextUTF(1,this->GetValue("filename"));

    bool fileDifferent(true);

    if(stmt.Step()==db::ReturnCode::Row){
        // File found in database.
        this->id    = stmt.ColumnInt(0);
        fileDifferent    = false;

        // Check if the track needs to be reread.
        if(fileSize!=stmt.ColumnInt(2) || fileTime!=stmt.ColumnInt(3)){
            fileDifferent    = true;
        }
    }

    return fileDifferent;
}






bool Track::Save(db::Connection &dbConnection,utfstring libraryDirectory,DBINT folderId){

    TrackMeta::TagMap metadataCopy(this->meta->tags);

    unsigned int count;

    db::ScopedTransaction transaction(dbConnection);

    //////////////////////////////////////////////////////////////////////////////
    // Start by removing existing relations
    if(this->id!=0){
        db::CachedStatement deleteGenre("DELETE FROM track_genres WHERE track_id=?",dbConnection);
        deleteGenre.BindInt(0,this->id);
        deleteGenre.Step();

        db::CachedStatement deleteArtist("DELETE FROM track_artists WHERE track_id=?",dbConnection);
        deleteArtist.BindInt(0,this->id);
        deleteArtist.Step();

        db::CachedStatement deleteMeta("DELETE FROM track_meta WHERE track_id=?",dbConnection);
        deleteMeta.BindInt(0,this->id);
        deleteMeta.Step();
    }

    //////////////////////////////////////////////////////////////////////////////
    // 1. Start by inserting as many tags as possible into the tracks-table
    {
        db::CachedStatement stmt("INSERT OR REPLACE INTO tracks (id,track,bpm,duration,filesize,year,folder_id,title,filename,filetime) VALUES (?,?,?,?,?,?,?,?,?,?)",dbConnection);

        // Bind all "static" tags (the once that are in the tracks tables)
        stmt.BindTextUTF(1,this->GetValue("track"));
        stmt.BindTextUTF(2,this->GetValue("bpm"));
        stmt.BindTextUTF(3,this->GetValue("duration"));
        stmt.BindTextUTF(4,this->GetValue("filesize"));
        stmt.BindTextUTF(5,this->GetValue("year"));
        stmt.BindInt(6,folderId);
        stmt.BindTextUTF(7,this->GetValue("title"));
        stmt.BindTextUTF(8,this->GetValue("filename"));
        stmt.BindTextUTF(9,this->GetValue("filetime"));

        if(this->id!=0){
            stmt.BindInt(0,this->id);
        }

        if(stmt.Step()==db::ReturnCode::Done){
            if(this->id==0){
                this->id    = dbConnection.LastInsertedId();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    // 2. Remove tags that has been used.
    metadataCopy.erase("track");
    metadataCopy.erase("bpm");
    metadataCopy.erase("duration");
    metadataCopy.erase("year");
    metadataCopy.erase("title");
    metadataCopy.erase("filename");
    metadataCopy.erase("filetime");
    metadataCopy.erase("filesize");
    metadataCopy.erase("title");
    metadataCopy.erase("path");
    metadataCopy.erase("extension");


    //////////////////////////////////////////////////////////////////////////////
    // 3. Read genres
    utfstring    visualGenres;
    DBINT            genreId(0);
    count            = 0;
    std::set<utfstring> alreadySetGenres;        // Cache for not saving duplicates

    TrackMeta::TagMapIteratorPair genres    = this->GetValues("genre");
    while(genres.first!=genres.second){

        if(alreadySetGenres.find(genres.first->second)==alreadySetGenres.end()){
            alreadySetGenres.insert(genres.first->second);

            // First save the genre
            genreId        = this->_GetGenre(dbConnection,genres.first->second,true);

            // Second, add to the visual genre
            if(count!=0){
                visualGenres    += UTF(", ");
            }
            visualGenres    += genres.first->second;

            ++count;
        }
        ++genres.first;
    }
    if(count>1 || genreId==0){
        // Check for new genre, but do not insert the relation, this is the visual_genre_id field in the database
        genreId        = this->_GetGenre(dbConnection,visualGenres,false,true);
    }
    metadataCopy.erase("genre");



    //////////////////////////////////////////////////////////////////////////////
    // 4. Read artists
    utfstring    visualArtists;
    DBINT            artistId(0);
    count            = 0;
    std::set<utfstring> alreadySetArtists;        // Cache for not saving duplicates

    TrackMeta::TagMapIteratorPair artists = this->GetValues("artist");
    while(artists.first!=artists.second){

        if(alreadySetArtists.find(artists.first->second)==alreadySetArtists.end()){
            alreadySetArtists.insert(artists.first->second);

            // First save the artist
            artistId        = this->_GetArtist(dbConnection,artists.first->second,true);

            // Second, add to the visual artist
            if(count!=0){
                visualArtists    += UTF(", ");
            }
            visualArtists    += artists.first->second;

            ++count;
        }

        ++artists.first;
    }
    if(count>1 || artistId==0){
        // Check for new artist, but do not insert the relation, this is the visual_artist_id field in the database
        artistId        = this->_GetArtist(dbConnection,visualArtists,false,true);
    }
    metadataCopy.erase("artist");


    //////////////////////////////////////////////////////////////////////////////
    // 5. Read album
    DBINT albumId(0);
    {
        db::CachedStatement stmt("SELECT id FROM albums WHERE name=?",dbConnection);
        stmt.BindTextUTF(0,this->GetValue("album"));

        if(stmt.Step()==db::ReturnCode::Row){
            albumId    = stmt.ColumnInt(0);
        }else{
            // INSERT a new album
            db::Statement insertAlbum("INSERT INTO albums (name) VALUES (?)",dbConnection);
            insertAlbum.BindTextUTF(0,this->GetValue("album"));
            if(insertAlbum.Step()==db::ReturnCode::Done){
                albumId    = dbConnection.LastInsertedId();
            }
        }
    }

    metadataCopy.erase("album");

    //////////////////////////////////////////////////////////////////////////////
    // 6. Thumbnail
    DBINT thumbnailId(0);

    if(this->meta->thumbnailData){
        UINT64 sum    = Checksum(this->meta->thumbnailData,this->meta->thumbnailSize);

        db::CachedStatement thumbs("SELECT id FROM thumbnails WHERE filesize=? AND checksum=?",dbConnection);
        thumbs.BindInt(0,this->meta->thumbnailSize);
        thumbs.BindInt64(1,sum);

        if(thumbs.Step()==db::ReturnCode::Row){
            thumbnailId    = thumbs.ColumnInt(0);
        }

        if(thumbnailId==0){
            // INSERT thumbnail
            db::Statement insertThumb("INSERT INTO thumbnails (filesize,checksum) VALUES (?,?)",dbConnection);
            insertThumb.BindInt(0,this->meta->thumbnailSize);
            insertThumb.BindInt64(1,sum);

            if(insertThumb.Step()==db::ReturnCode::Done){
                thumbnailId    = dbConnection.LastInsertedId();
            }

            // Save the file
            utfstring filename    = libraryDirectory+UTF("thumbs/")+boost::lexical_cast<utfstring>(thumbnailId)+UTF(".jpg");

            // TODO, fix for UTF wide of not
            FILE *thumbFile        = _wfopen(filename.c_str(),UTF("wb"));
            fwrite(this->meta->thumbnailData,sizeof(char),this->meta->thumbnailSize,thumbFile);
            fclose(thumbFile);

        }

    }

    //////////////////////////////////////////////////////////////////////////////
    // 5. Update tracks table with relations
    {
        db::CachedStatement stmt("UPDATE tracks SET album_id=?,visual_genre_id=?,visual_artist_id=?,thumbnail_id=? WHERE id=?",dbConnection);
        stmt.BindInt(0,albumId);
        stmt.BindInt(1,genreId);
        stmt.BindInt(2,artistId);
        stmt.BindInt(3,thumbnailId);
        stmt.BindInt(4,this->id);
        stmt.Step();
    }


    //////////////////////////////////////////////////////////////////////////////
    // 6. Fields that are left in tagsCopy should be meta-data (extra tags)

    {
        db::CachedStatement selectMetaKey("SELECT id FROM meta_keys WHERE name=?",dbConnection);

        db::CachedStatement selectMetaValue("SELECT id FROM meta_values WHERE meta_key_id=? AND content=?",dbConnection);
        db::CachedStatement insertMetaValue("INSERT INTO meta_values (meta_key_id,content) VALUES (?,?)",dbConnection);

        db::CachedStatement insertTrackMeta("INSERT INTO track_meta (track_id,meta_value_id) VALUES (?,?)",dbConnection);

        // Loop through the tags
        for(TrackMeta::TagMap::const_iterator metaData=metadataCopy.begin();metaData!=metadataCopy.end();++metaData){

            // 1. Find the meta_key
            DBINT metaKeyId(0);
            std::string key;

            selectMetaKey.BindText(0,metaData->first);

            if(selectMetaKey.Step()==db::ReturnCode::Row){
                // key found
                metaKeyId    = selectMetaKey.ColumnInt(0);
            }else{
                // key not found, INSERT
                db::CachedStatement insertMetaKey("INSERT INTO meta_keys (name) VALUES (?)",dbConnection);
                insertMetaKey.BindText(0,metaData->first);
                if(insertMetaKey.Step()==db::ReturnCode::Done){
                    metaKeyId    = dbConnection.LastInsertedId();
                }
            }
            selectMetaKey.Reset();


            // 2. Find meta value
            if(metaKeyId!=0){
                DBINT metaValueId(0);
                // 2.1 Find meta_value
                selectMetaValue.BindInt(0,metaKeyId);
                selectMetaValue.BindTextUTF(1,metaData->second);

                if(selectMetaValue.Step()==db::ReturnCode::Row){
                    // Value found
                    metaValueId    = selectMetaValue.ColumnInt(0);
                }else{
                    // Value not found, INSERT
                    insertMetaValue.BindInt(0,metaKeyId);
                    insertMetaValue.BindTextUTF(1,metaData->second);
                    if(insertMetaValue.Step()==db::ReturnCode::Done){
                        metaValueId    = dbConnection.LastInsertedId();
                    }
                    insertMetaValue.Reset();
                }
                selectMetaValue.Reset();

                // 3. INSERT into the track_meta table
                if(metaValueId!=0){
                    insertTrackMeta.BindInt(0,this->id);
                    insertTrackMeta.BindInt(1,metaValueId);
                    insertTrackMeta.Step();
                    insertTrackMeta.Reset();
                }
            }

        }

    }

    return true;
}

DBINT Track::_GetGenre(db::Connection &dbConnection,utfstring genre,bool addRelation,bool aggregated){
    DBINT genreId(0);
    {
        db::CachedStatement stmt("SELECT id FROM genres WHERE name=?",dbConnection);
        stmt.BindTextUTF(0,genre);
        if(stmt.Step()==db::ReturnCode::Row){
            genreId    = stmt.ColumnInt(0);
        }
    }

    if(genreId==0){
        // Insert the genre
        DBINT aggregatedInt    = (aggregated?1:0);

        db::CachedStatement stmt("INSERT INTO genres (name,aggregated) VALUES (?,?)",dbConnection);
        stmt.BindTextUTF(0,genre);
        stmt.BindInt(1,aggregatedInt);

        if(stmt.Step()==db::ReturnCode::Done){
            genreId        = dbConnection.LastInsertedId();
        }
    }

    if(addRelation){
        // Insert into track_genres
        db::Statement stmt("INSERT INTO track_genres (track_id,genre_id) VALUES (?,?)",dbConnection);
        stmt.BindInt(0,this->id);
        stmt.BindInt(1,genreId);
        stmt.Step();
    }

    return genreId;
}

DBINT Track::_GetArtist(db::Connection &dbConnection,utfstring artist,bool addRelation,bool aggregated){
    DBINT artistId(0);

    db::CachedStatement stmt("SELECT id FROM artists WHERE name=?",dbConnection);
    stmt.BindTextUTF(0,artist);

    if(stmt.Step()==db::ReturnCode::Row){
        artistId    = stmt.ColumnInt(0);
    }

    if(artistId==0){
        // Insert the genre
        DBINT aggregatedInt    = (aggregated?1:0);
        db::Statement insertArtist("INSERT INTO artists (name,aggregated) VALUES (?,?)",dbConnection);
        insertArtist.BindTextUTF(0,artist);
        insertArtist.BindInt(1,aggregatedInt);

        if(insertArtist.Step()==db::ReturnCode::Done){
            artistId        = dbConnection.LastInsertedId();
        }
    }

    if(addRelation){
        // Insert into track_genres
        db::CachedStatement insertRel("INSERT INTO track_artists (track_id,artist_id) VALUES (?,?)",dbConnection);
        insertRel.BindInt(0,this->id);
        insertRel.BindInt(1,artistId);
        insertRel.Step();
    }

    return artistId;
}

void Track::SetThumbnail(const char *data,unsigned int size){
    if(this->meta){
        this->meta->SetThumbnail(data,size);
    }
}

TrackPtr Track::Copy(){
    return TrackPtr(new Track(this->id));
}
