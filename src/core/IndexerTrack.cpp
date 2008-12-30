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

#include <core/IndexerTrack.h>

#include <core/Common.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/Library/Base.h>
#include <core/filestreams/Factory.h>

#include <boost/lexical_cast.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

IndexerTrack::IndexerTrack(DBINT id)
 :meta(NULL)
 ,id(id)
{
}


IndexerTrack::~IndexerTrack(void){
    if(this->meta){
        delete this->meta;
        this->meta  = NULL;
    }
}

const utfchar* IndexerTrack::GetValue(const char* metakey){
    if(metakey && this->meta){
        MetadataMap::iterator metavalue = this->meta->metadata.find(metakey);
        if(metavalue!=this->meta->metadata.end()){
            return metavalue->second.c_str();
        }
    }
    return NULL;
}

void IndexerTrack::SetValue(const char* metakey,const utfchar* value){
    this->InitMeta();
    if(metakey && value){
        this->meta->metadata.insert(std::pair<std::string,utfstring>(metakey,value));
    }
}

void IndexerTrack::ClearValue(const char* metakey){
    if(this->meta){
        this->meta->metadata.erase(metakey);
    }
}



void IndexerTrack::SetThumbnail(const char *data,long size){
    this->InitMeta();

    if(this->meta->thumbnailData)
        delete this->meta->thumbnailData;

    this->meta->thumbnailData        = new char[size];
    this->meta->thumbnailSize        = size;

    memcpy(this->meta->thumbnailData,data,size);
}

const utfchar* IndexerTrack::URI(){
    return NULL;
}

const utfchar* IndexerTrack::URL(){
    return this->GetValue("path");
}

Track::MetadataIteratorRange IndexerTrack::GetValues(const char* metakey){
    if(this->meta){
        return this->meta->metadata.equal_range(metakey);
    }
    return Track::MetadataIteratorRange();
}

Track::MetadataIteratorRange IndexerTrack::GetAllValues(){
    if(this->meta){
        return Track::MetadataIteratorRange(this->meta->metadata.begin(),this->meta->metadata.end());
    }
    return Track::MetadataIteratorRange();
}

DBINT IndexerTrack::Id(){
    return this->id;
}

void IndexerTrack::InitMeta(){
    if(!this->meta){
        // Create the metadata
        this->meta  = new MetaData();
    }
}


bool IndexerTrack::CompareDBAndFileInfo(const boost::filesystem::utfpath &file,db::Connection &dbConnection,DBINT currentFolderId){
 
    try{
        this->SetValue("path",file.string().c_str());
        this->SetValue("filename",file.leaf().c_str());

        utfstring::size_type lastDot = file.leaf().find_last_of(UTF("."));
        if(lastDot!=utfstring::npos){
            this->SetValue("extension",file.leaf().substr(lastDot+1).c_str());
        }

        DBINT fileSize  = (DBINT)boost::filesystem::file_size(file);
        DBTIME fileTime = (DBTIME)boost::filesystem::last_write_time(file);

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
    }catch(...){
    }
    return false;
}



bool IndexerTrack::Save(db::Connection &dbConnection,utfstring libraryDirectory,DBINT folderId){

    MetadataMap metadataCopy(this->meta->metadata);

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

    MetadataIteratorRange genres    = this->GetValues("genre");
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

    MetadataIteratorRange artists = this->GetValues("artist");
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
        const utfchar *album=this->GetValue("album");
        if(album==NULL){
            album=UTF("");
        }

        stmt.BindTextUTF(0,album);

        if(stmt.Step()==db::ReturnCode::Row){
            albumId    = stmt.ColumnInt(0);
        }else{
            // INSERT a new album
            db::Statement insertAlbum("INSERT INTO albums (name) VALUES (?)",dbConnection);
            insertAlbum.BindTextUTF(0,album);

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
        for(MetadataMap::const_iterator metaData=metadataCopy.begin();metaData!=metadataCopy.end();++metaData){

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

DBINT IndexerTrack::_GetGenre(db::Connection &dbConnection,utfstring genre,bool addRelation,bool aggregated){
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

DBINT IndexerTrack::_GetArtist(db::Connection &dbConnection,utfstring artist,bool addRelation,bool aggregated){
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


TrackPtr IndexerTrack::Copy(){
    return TrackPtr(new IndexerTrack(this->id));
}



IndexerTrack::MetaData::MetaData()
 :thumbnailData(NULL)
 ,thumbnailSize(0)
{
}


IndexerTrack::MetaData::~MetaData(){
    if(this->thumbnailData)
        delete this->thumbnailData;
}

bool IndexerTrack::GetTrackMetadata(db::Connection &db){
    db::Statement genres("SELECT g.name FROM genres g,track_genres tg WHERE tg.genre_id=g.id AND tg.track_id=? ORDER BY tg.id",db);
    db::Statement artists("SELECT ar.name FROM artists ar,track_artists ta WHERE ta.artist_id=ar.id AND ta.track_id=? ORDER BY ta.id",db);
    db::Statement allMetadata("SELECT mv.content,mk.name FROM meta_values mv,meta_keys mk,track_meta tm WHERE tm.track_id=? AND tm.meta_value_id=mv.id AND mv.meta_key_id=mk.id ORDER BY tm.id",db);
    db::Statement track("SELECT t.track,t.bpm,t.duration,t.filesize,t.year,t.title,t.filename,t.thumbnail_id,p.path||f.relative_path||'/'||t.filename,al.name FROM tracks t,folders f,paths p,albums al WHERE t.id=? AND t.folder_id=f.id AND f.path_id=p.id AND t.album_id=al.id",db);

    track.BindInt(0,this->id);
    if(track.Step()==db::Row){
        this->SetValue("track",track.ColumnTextUTF(0));
        this->SetValue("bpm",track.ColumnTextUTF(1));
        this->SetValue("duration",track.ColumnTextUTF(2));
        this->SetValue("filesize",track.ColumnTextUTF(3));
        this->SetValue("year",track.ColumnTextUTF(4));
        this->SetValue("title",track.ColumnTextUTF(5));
        this->SetValue("filename",track.ColumnTextUTF(6));
        this->SetValue("thumbnail_id",track.ColumnTextUTF(7));
        this->SetValue("path",track.ColumnTextUTF(8));
        this->SetValue("album",track.ColumnTextUTF(9));

        // genres
        genres.BindInt(0,this->id);
        while(genres.Step()==db::Row){
            this->SetValue("genre",genres.ColumnTextUTF(0));
        }
        // artists
        artists.BindInt(0,this->id);
        while(artists.Step()==db::Row){
            this->SetValue("artist",artists.ColumnTextUTF(0));
        }
        // other metadata
        allMetadata.BindInt(0,this->id);
        while(allMetadata.Step()==db::Row){
            this->SetValue(allMetadata.ColumnText(1),allMetadata.ColumnTextUTF(0));
        }

        return true;
    }
    return false;
}
