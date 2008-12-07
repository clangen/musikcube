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

#pragma once

#include <core/config_filesystem.h>
#include <core/Track.h>
#include <core/Library/Base.h>

//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{ namespace http{
    class Responder;
} } }
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
namespace musik{ namespace core{
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///A LibraryTrack is a track related to a Library.
//////////////////////////////////////////
class LibraryTrack : public Track {
    public:
        LibraryTrack(void);
        LibraryTrack(DBINT id,int libraryId);
        LibraryTrack(DBINT id,musik::core::LibraryPtr library);
        virtual ~LibraryTrack(void);

        virtual const utfchar* GetValue(const char* metakey);
        virtual void SetValue(const char* metakey,const utfchar* value);
        virtual void ClearValue(const char* metakey);
        virtual void SetThumbnail(const char *data,long size);
        virtual const utfchar* URI();
        virtual const utfchar* URL();

        virtual MetadataIteratorRange GetValues(const char* metakey);
        virtual MetadataIteratorRange GetAllValues();
        virtual TrackPtr Copy();

        virtual DBINT Id();
        virtual musik::core::LibraryPtr Library();
        virtual int LibraryId();

    private:
        // The variables
        DBINT id;
        int libraryId;
    private:
        class MetaData{
            public:
                MetaData();
                ~MetaData();

                Track::MetadataMap metadata;
                char *thumbnailData;
                long thumbnailSize;
                musik::core::LibraryPtr library;
        };

        MetaData *meta;

    private:

        void InitMeta();

        // Some special methods for the Indexer
        friend class Indexer;

        bool CompareDBAndFileInfo(const boost::filesystem::utfpath &file,db::Connection &dbConnection,DBINT currentFolderId);
        bool Save(db::Connection &dbConnection,utfstring libraryDirectory,DBINT folderId);
        DBINT _GetGenre(db::Connection &dbConnection,utfstring genre,bool addRelation,bool aggregated=false);
        DBINT _GetArtist(db::Connection &dbConnection,utfstring artist,bool addRelation,bool aggregated=false);

    private:
        // Some special methods for the http::Responder
        friend class http::Responder;
        bool GetFileData(DBINT id,db::Connection &db);
};


//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////


