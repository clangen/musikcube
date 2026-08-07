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

/** @file TrackListQueryBase.h
 *  @brief Base class for queries that return a track list.
 *  @details Defines the result types (track list, column headers, durations) and
 *      the serialization helpers shared by all track-list queries, plus the
 *      SDK wrapper used to expose results to plugins. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/db/Connection.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/query/util/Serialization.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Common base for track-list returning queries.
     *  @details Subclasses fill a Result track list, a Headers set describing
     *      which columns are populated, and a Durations map. Provides limit/
     *      offset pagination and JSON (de)serialization support. */
    class TrackListQueryBase : public musik::core::library::query::QueryBase {
        public:
            typedef std::shared_ptr<musik::core::TrackList> Result; /**< Result track list. */
            typedef std::shared_ptr<std::set<size_t>> Headers; /**< Populated column indices. */
            typedef std::shared_ptr<std::map<size_t, size_t>> Durations; /**< index -> seconds map. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(TrackListQueryBase)

            /** @brief Creates a query with default pagination (no limit). */
            TrackListQueryBase() {
                this->limit = -1;
                this->offset = 0;
            }

            /* virtual methods we define */
            /** @return The result track list. */
            virtual Result GetResult() = 0;
            /** @return The populated column headers. */
            virtual Headers GetHeaders() = 0;
            /** @return The track durations. */
            virtual Durations GetDurations() = 0;
            /** @return A hash identifying this query's parameters. */
            virtual size_t GetQueryHash() = 0;

            /** @brief Sets the result pagination.
             *  @param limit Maximum results (-1 for all).
             *  @param offset Result offset. */
            virtual void SetLimitAndOffset(int limit, int offset = 0) noexcept {
                this->limit = limit;
                this->offset = offset;
            }

            /** @return The result wrapped as a raw SDK ITrackList.
             *  @note The caller owns the returned object. */
            virtual musik::core::sdk::ITrackList* GetSdkResult() {
                return new WrappedTrackList(GetResult());
            }

        protected:

            /* for IMetadataProxy */

            /** @return A SQL "LIMIT x OFFSET y" fragment, or empty when not paginating. */
            std::string GetLimitAndOffset() {
                if (this->limit > 0 && this->offset >= 0) {
                    return u8fmt("LIMIT %d OFFSET %d", this->limit, this->offset);
                }
                return "";
            }

            /* for ISerialization */

            /** @brief Finalizes serialization with the current pagination.
             *  @param output The JSON document being built.
             *  @return The serialized JSON string. */
            const std::string FinalizeSerializedQueryWithLimitAndOffset(nlohmann::json &output) {
                auto& options = output["options"];
                options["limit"] = this->limit;
                options["offset"] = this->offset;
                return output.dump();
            }

            /** @brief Restores pagination from a deserialized query.
             *  @param options The JSON options object. */
            void ExtractLimitAndOffsetFromDeserializedQuery(const nlohmann::json& options) {
                this->limit = options.value("limit", -1);
                this->offset = options.value("offset", 0);
            }

            /** @brief Builds a JSON result containing headers, durations and the track list.
             *  @return The initialized JSON document. */
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

            /** @brief Fills a query's result from a deserialized JSON result.
             *  @param result The JSON result document.
             *  @param library Library used to resolve source info.
             *  @param query The query to populate. */
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
            int limit, offset; /**< Pagination values. */

            /** @brief Adapts an internal TrackList to the SDK ITrackList interface. */
            class WrappedTrackList : public musik::core::sdk::ITrackList {
                public:
                    /** @brief Wraps an internal track list.
                     *  @param wrapped The track list to adapt. */
                    WrappedTrackList(Result wrapped) noexcept {
                        this->wrapped = wrapped;
                    }

                    virtual ~WrappedTrackList() {
                    }

                    /** @brief Frees the wrapper (deletes this instance). */
                    void Release() noexcept override {
                        delete this;
                    }

                    /** @return The number of tracks in the list. */
                    size_t Count() const override {
                        return this->wrapped->Count();
                    }

                    /** @return The id of the track at the given index.
                     *  @param index Zero-based index. */
                    int64_t GetId(size_t index) const override {
                        return this->wrapped->GetId(index);
                    }

                    /** @return The index of the track with the given id, or -1.
                     *  @param id The track id. */
                    int IndexOf(int64_t id) const override {
                        return this->wrapped->IndexOf(id);
                    }

                    /** @return The track at the given index, or nullptr.
                     *  @param index Zero-based index. */
                    musik::core::sdk::ITrack* GetTrack(size_t index) const override  {
                        return this->wrapped->GetTrack(index);
                    }

                private:
                    Result wrapped; /**< Wrapped internal track list. */
            };
    };

} } } }
