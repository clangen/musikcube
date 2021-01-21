//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/ILibrary.h>
#include <unordered_set>
#include <unordered_map>

namespace musik { namespace core { namespace library { namespace query {

class TrackMetadataBatchQuery: public QueryBase {
    public:
        static const std::string kQueryName;

        using IdToTrackMap = std::unordered_map<int64_t, TrackPtr>;

        TrackMetadataBatchQuery(
            std::unordered_set<int64_t> trackIds,
            musik::core::ILibraryPtr library);

        virtual ~TrackMetadataBatchQuery() {
        }

        const IdToTrackMap& Result() {
            return this->result;
        }

        virtual std::string Name() { return kQueryName; }

        /* ISerializableQuery */
        virtual std::string SerializeQuery();
        virtual std::string SerializeResult();
        virtual void DeserializeResult(const std::string& data);

        static std::shared_ptr<TrackMetadataBatchQuery> DeserializeQuery(
            musik::core::ILibraryPtr library, const std::string& data);

    protected:
        virtual bool OnRun(musik::core::db::Connection& db);

    private:
        musik::core::ILibraryPtr library;
        std::unordered_set<int64_t> trackIds;
        IdToTrackMap result;
};

} } } }