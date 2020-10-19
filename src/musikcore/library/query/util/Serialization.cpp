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

#include "pch.hpp"
#include "Serialization.h"
#include <musikcore/library/track/LibraryTrack.h>

using namespace musik::core;
using namespace musik::core::library::query;
using namespace musik::core::sdk;

namespace musik { namespace core { namespace library { namespace query {

    namespace serialization {

        nlohmann::json PredicateListToJson(const category::PredicateList& input) {
            nlohmann::json result;
            for (auto& it : input) {
                result.push_back({ it.first, it.second });
            }
            return result;
        }

        void PredicateListFromJson(const nlohmann::json& input, category::PredicateList& output) {
            for (auto& it : input) {
                output.push_back({ it[0], it[1] });
            }
        }

        nlohmann::json MetadataMapListToJson(const MetadataMapList& input) {
            nlohmann::json result;
            for (size_t i = 0; i < input.Count(); i++) {
                nlohmann::json outputMetadata;
                auto inputMap = input.GetSharedAt(i);
                inputMap->Each([&outputMetadata](const std::string& key, const std::string& value) {
                    outputMetadata[key] = value;
                });
                result.push_back({
                    { "id", inputMap->GetId() },
                    { "value", inputMap->GetTypeValue() },
                    { "type", inputMap->GetType() },
                    { "metadata", outputMetadata }
                });
            }
            return result;
        }

        void MetadataMapListFromJson(const nlohmann::json& input, MetadataMapList& output) {
            output.Clear();
            for (size_t i = 0; i < input.size(); i++) {
                auto& element = input[i];
                auto outputMap = std::make_shared<MetadataMap>(
                    element.value("id", -1LL),
                    element.value("value", "unknown"),
                    element.value("type", "unknown"));
                auto& metadata = element["metadata"];
                for (auto& kv : metadata.items()) {
                    outputMap->Set(kv.key().c_str(), kv.value().get<std::string>().c_str());
                }
                output.Add(outputMap);
            }
        }

        nlohmann::json ValueListToJson(const SdkValueList& input) {
            nlohmann::json result;
            input.Each([&result](auto value) {
                result.push_back({
                    { "value", value->ToString() },
                    { "id", value->GetId() },
                    { "type", value->GetType() }
                });
            });
            return result;
        }

        void ValueListFromJson(const nlohmann::json& input, SdkValueList& output) {
            output.Clear();
            for (auto& v : input) {
                output.Add(std::make_shared<SdkValue>(
                    v["value"],
                    v["id"],
                    v["type"]
               ));
            }
        }

        #define COPY_TRACK_FIELD_TO_JSON(track, json, field) \
            json[field] = track->GetString(field);

        nlohmann::json TrackToJson(const TrackPtr input, bool onlyIds) {
            nlohmann::json output;

            output["id"] = input->GetId();
            COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::EXTERNAL_ID)
            COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::SOURCE_ID)

            if (!onlyIds) {
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::TRACK_NUM)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::DISC_NUM)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::BPM)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::DURATION)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::FILESIZE)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::TITLE)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::FILENAME)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::THUMBNAIL_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ALBUM)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ALBUM_ARTIST)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::GENRE)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ARTIST)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::FILETIME)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::GENRE_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ARTIST_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ALBUM_ARTIST_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::ALBUM_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::SOURCE_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::EXTERNAL_ID)
                COPY_TRACK_FIELD_TO_JSON(input, output, constants::Track::RATING)

                auto replayGain = input->GetReplayGain();
                output["replayGain"] = {
                    { "albumGain", replayGain.albumGain },
                    { "albumPeak", replayGain.albumPeak },
                    { "trackGain", replayGain.trackGain },
                    { "trackPeak", replayGain.trackPeak }
                };
            }

            return output;
        }

        #define COPY_JSON_FIELD_TO_TRACK(json, track, field) \
            track->SetValue(field, json.value(field, "").c_str());

        void TrackFromJson(const nlohmann::json& input, musik::core::TrackPtr output, bool onlyIds) {
            output->SetId(input["id"].get<int64_t>());
            COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::EXTERNAL_ID)
            COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::SOURCE_ID)

            if (!onlyIds) {
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::TRACK_NUM)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::DISC_NUM)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::BPM)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::DURATION)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::FILESIZE)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::TITLE)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::FILENAME)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::THUMBNAIL_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ALBUM)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ALBUM_ARTIST)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::GENRE)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ARTIST)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::FILETIME)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::GENRE_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ARTIST_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ALBUM_ARTIST_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::ALBUM_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::SOURCE_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::EXTERNAL_ID)
                COPY_JSON_FIELD_TO_TRACK(input, output, constants::Track::RATING)

                auto replayGainJson = input["replayGain"];
                replayGainJson = replayGainJson.is_null() ? nlohmann::json() : replayGainJson;
                musik::core::sdk::ReplayGain replayGain;
                replayGain.albumGain = replayGainJson.value("albumGain", 1.0f);
                replayGain.albumPeak = replayGainJson.value("albumPeak", 1.0f);
                replayGain.trackGain = replayGainJson.value("trackGain", 1.0f);
                replayGain.trackPeak = replayGainJson.value("trackPeak", 1.0f);
                output->SetReplayGain(replayGain);
            }

            output->SetMetadataState(MetadataState::Loaded);
        }

        nlohmann::json TrackListToJson(const TrackList& input, bool onlyIds) {
            nlohmann::json output;
            if (onlyIds) {
                output = input.GetIds();
            }
            else {
                for (size_t i = 0; i < input.Count(); i++) {
                    output.push_back(TrackToJson(input.Get(i), onlyIds));
                }
            }
            return output;
        }

        void TrackListFromJson(const nlohmann::json& input, TrackList& output, ILibraryPtr library, bool onlyIds) {
            output.Clear();
            if (onlyIds) {
                for (auto& trackId : input) {
                    output.Add(trackId.get<int64_t>());
                }
            }
            else {
                for (auto& trackJson : input) {
                    output.Add(trackJson["id"].get<int64_t>());
                }
            }
        }

        nlohmann::json ITrackListToJsonIdList(const ITrackList& input) {
            nlohmann::json output;
            for (size_t i = 0; i < input.Count(); i++) {
                output.push_back(input.GetId(i));
            }
            return output;
        }

    }

} } } }
