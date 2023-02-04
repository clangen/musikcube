//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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
#include <musikcore/db/Connection.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/query/util/Serialization.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace musik { namespace core { namespace library { namespace query {

    class TrackListQueryBase : public musik::core::library::query::QueryBase {
        public:
            typedef std::shared_ptr<musik::core::TrackList> Result;
            typedef std::shared_ptr<std::set<size_t>> Headers;
            typedef std::shared_ptr<std::map<size_t, size_t>> Durations;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(TrackListQueryBase)

            TrackListQueryBase() {
                this->limit = -1;
                this->offset = 0;
            }

            /* virtual methods we define */
            virtual Result GetResult() = 0;
            virtual Headers GetHeaders() = 0;
            virtual Durations GetDurations() = 0;
            virtual size_t GetQueryHash() = 0;

            virtual void SetLimitAndOffset(int limit, int offset = 0) noexcept {
                this->limit = limit;
                this->offset = offset;
            }

            virtual musik::core::sdk::ITrackList* GetSdkResult() {
                return new WrappedTrackList(GetResult());
            }

        protected:

            /* for IMetadataProxy */

            std::string GetLimitAndOffset() {
                if (this->limit > 0 && this->offset >= 0) {
                    return u8fmt("LIMIT %d OFFSET %d", this->limit, this->offset);
                }
                return "";
            }

            /* for ISerialization */

            const std::string FinalizeSerializedQueryWithLimitAndOffset(nlohmann::json &output) {
                auto& options = output["options"];
                options["limit"] = this->limit;
                options["offset"] = this->offset;
                return output.dump();
            }

            void ExtractLimitAndOffsetFromDeserializedQuery(const nlohmann::json& options) {
                this->limit = options.value("limit", -1);
                this->offset = options.value("offset", 0);
            }

            nlohmann::json InitializeSerializedResultWithHeadersAndTrackList() {
                nlohmann::json output = {
                    { "result", {
                        { "headers", *this->GetHeaders() },
                        { "durations", serialization::DurationMapToJsonMap(*this->GetDurations()) },
                        { "trackList", serialization::TrackListToJson(*this->GetResult(), true) }
                    }}
                };
                return output;
            }

            void DeserializeTrackListAndHeaders(
                nlohmann::json& result,
                ILibraryPtr library,
                TrackListQueryBase* query)
            {
                serialization::JsonArrayToSet<std::set<size_t>, size_t>(result["headers"], *query->GetHeaders());
                serialization::JsonMapToDuration(result["durations"], *query->GetDurations());
                serialization::TrackListFromJson(result["trackList"], *query->GetResult(), library, true);
            }

        private:
            int limit, offset;

            class WrappedTrackList : public musik::core::sdk::ITrackList {
                public:
                    WrappedTrackList(Result wrapped) noexcept {
                        this->wrapped = wrapped;
                    }

                    virtual ~WrappedTrackList() {
                    }

                    void Release() noexcept override {
                        delete this;
                    }

                    size_t Count() const override {
                        return this->wrapped->Count();
                    }

                    int64_t GetId(size_t index) const override {
                        return this->wrapped->GetId(index);
                    }

                    int IndexOf(int64_t id) const override {
                        return this->wrapped->IndexOf(id);
                    }

                    musik::core::sdk::ITrack* GetTrack(size_t index) const override  {
                        return this->wrapped->GetTrack(index);
                    }

                private:
                    Result wrapped;
            };
    };

} } } }
