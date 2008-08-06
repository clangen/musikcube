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

#include <core/TrackMeta.h>
#include <core/ITrack.h>

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>


//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{
    class Track;
    namespace Library{
        class Base;
    }
    namespace db{
        class Connection;
    }
} }
//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

/*!
 * \brief
 * shared pointer to a track.
 *
 * The TrackPtr is used almost everywhere since the Tracks can be
 * shared between different threads.
 *
 * \remarks
 * For the Tracks to be thread-safe, you need to pass the ThreadHelper object when required.
 *
 * \see
 * Track|TrackVector
 */
typedef boost::shared_ptr<Track> TrackPtr;

/*!
 * \brief
 * A vector of TrackPtr
 *
 * \see
 * Track|TrackPtr
 */
typedef std::vector<TrackPtr> TrackVector;

/*!
 * \brief
 * Track is the main class for handling Tracks.
 *
 * \see
 * TrackPtr|TrackVector
 */
class Track : public ITrack {
    public:
        Track(void);
        Track(DBINT newId);
        ~Track(void);


        TrackMeta::TagMapIteratorPair GetAllValues() const;

        //void SetValue(const TrackMeta::Key &key,TrackMeta::Value &value);

        TrackMeta::TagMapIteratorPair GetValues(const char* metakey) const;

        const utfchar* GetValue(const char* metakey) const;
        void SetValue(const char* metakey,const utfchar* value);
        void SetThumbnail(const char *data,unsigned int size);


        void InitMeta(Library::Base *library);
        bool HasMeta();

        DBINT id;

        void ClearMeta();
        TrackPtr Copy();

    private:


        /*!
         * \brief
         * A pointer to a std::multimap of all required tags.
         *
         * \remarks
         * The reason that this is a pointer and not a multimap on it's own
         * is that we try to keep the Track as small as possible since there
         * may be several thousand in use at the same time
         */
        TrackMeta *meta;


        inline const TrackMeta::Value& _GetValue(const TrackMeta::Key & key) const;

    private:
        friend class Indexer;

        bool CompareDBAndFileInfo(const boost::filesystem::utfpath &file,db::Connection &dbConnection,DBINT currentFolderId);
        bool Save(db::Connection &dbConnection,utfstring libraryDirectory,DBINT folderId);
        DBINT _GetGenre(db::Connection &dbConnection,utfstring genre,bool addRelation,bool aggregated=false);
        DBINT _GetArtist(db::Connection &dbConnection,utfstring artist,bool addRelation,bool aggregated=false);
};


//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////


