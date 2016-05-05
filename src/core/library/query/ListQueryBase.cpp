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

#include <core/library/query/ListQueryBase.h>
#include <core/library/LibraryBase.h>
#include <core/support/Common.h>
#include <core/library/track/LibraryTrack.h>
#include <boost/algorithm/string.hpp>

using namespace musik::core;
using namespace musik::core::query;

ListQueryBase::ListQueryBase() 
: clearedTrackResults(false)
, trackInfoTracks(0)
, trackInfoDuration(0)
, trackInfoSize(0) {
}

ListQueryBase::~ListQueryBase() {
}

bool ListQueryBase::RunCallbacks(library::LibraryBase *library) {
    bool result = false;

    MetadataResults metadataResultsCopy;
    TrackVector trackResultsCopy;

    {
        /* scope for swapping the results safely */
        boost::mutex::scoped_lock lock(library->resultMutex);
        metadataResultsCopy.swap(this->metadataResults);
        trackResultsCopy.swap(this->trackResults);
    }

    {
        boost::mutex::scoped_lock lock(library->libraryMutex);

        /* query may already be finished */
        if ((this->status & QueryBase::Ended) != 0) {
            result = true;
        }
    }

    /* see if we have some results */
    if (metadataResultsCopy.size() != 0){
        MetadataResults::iterator it = metadataResultsCopy.begin();

        for ( ; it != metadataResultsCopy.end(); ++it) {

            std::string metatag(it->first);

            // check if the signal should send the clear flag
            bool clearMetatag = false;

            if (this->clearedMetadataResults.find(metatag) == this->clearedMetadataResults.end()) {
                clearMetatag = true;
                this->clearedMetadataResults.insert(metatag);    // Set this to cleared
            }

            this->metadataEvent[metatag](&it->second,clearMetatag);
        }
    }

    /* any tracks? */
    if(!trackResultsCopy.empty()) {
        /* notify */
        this->trackEvent(&trackResultsCopy, !this->clearedTrackResults);

        if (!this->clearedTrackResults) {
            this->clearedTrackResults = true;
        }
    }

    if (result) {
        boost::mutex::scoped_lock lock(library->resultMutex);
        this->trackInfoEvent(trackInfoTracks,trackInfoDuration,trackInfoSize);
    }

    return result;
}

ListQueryBase::MetadataSignal& ListQueryBase::OnMetadataEvent(const char* metatag){
    return this->metadataEvent[metatag];
}

ListQueryBase::TrackSignal& ListQueryBase::OnTrackEvent(){
    return this->trackEvent;
}

ListQueryBase::TrackInfoSignal& ListQueryBase::OnTrackInfoEvent(){
    return this->trackInfoEvent;
}

bool ListQueryBase::ParseTracksSQL(std::string sql, library::LibraryBase *library, db::Connection &db) {
    if (this->trackEvent.has_connections() && !library->QueryCanceled(this)) {
        db::Statement selectTracks(sql.c_str(), db);

        TrackVector tempTrackResults;
        tempTrackResults.reserve(101);
        int row(0);
        while (selectTracks.Step() == db::Row) {
            tempTrackResults.push_back(TrackPtr(new LibraryTrack(selectTracks.ColumnInt(0), library->Id())));
            this->trackInfoDuration += selectTracks.ColumnInt64(1);
            this->trackInfoSize += selectTracks.ColumnInt64(2);
            this->trackInfoTracks++;

            if ((++row) % 100 == 0) {
                boost::mutex::scoped_lock lock(library->resultMutex);
                this->trackResults.insert(this->trackResults.end(), tempTrackResults.begin(), tempTrackResults.end());

                tempTrackResults.clear();
                trackResults.reserve(101);
            }
        }
        if (!tempTrackResults.empty()) {
            boost::mutex::scoped_lock lock(library->resultMutex);
            this->trackResults.insert(this->trackResults.end(), tempTrackResults.begin(), tempTrackResults.end());
        }

        return true;
    }
    else {
        return false;
    }

}

