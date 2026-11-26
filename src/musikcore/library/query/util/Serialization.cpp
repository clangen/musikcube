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

        nlohmann::json MetadataMapsToJson(const std::vector<AlbumRecord>& input) {
            nlohmann::json result;
            for (const auto& album : input) {
                nlohmann::json metadata;
                for (const auto& [key, value] : album.metadata) {
                    metadata[key] = value;
                }
                result.push_back({
                    { "id", album.id },
                    { "value", album.value },
                    { "type", album.type },
                    { "metadata", metadata },
                });
            }
            return result;
        }

        void MetadataMapsFromJson(const nlohmann::json& input, std::vector<AlbumRecord>& output) {
            output.clear();
            for (const auto& element : input) {
                AlbumRecord album {
                    .id = element.value("id", -1LL),
                    .value = element.value("value", "unknown"),
                    .type = element.value("type", "unknown"),
                };
                for (const auto& kv : element["metadata"].items()) {
                    album.metadata.emplace(kv.key(), kv.value().get<std::string>());
                }
                output.push_back(std::move(album));
            }
        }

        nlohmann::json ValueListToJson(const CategoryValues& input) {
            nlohmann::json result;
            for (const auto& value : input) {
                result.push_back({
                    { "value", value.value },
                    { "id", value.id },
                    { "type", value.type },
                    { "source_id", value.sourceId ? nlohmann::json(*value.sourceId) : nlohmann::json() },
                });
            }
            return result;
        }

        void ValueListFromJson(const nlohmann::json& input, CategoryValues& output) {
            output.clear();
            for (const auto& value : input) {
                output.push_back({
                    .value = value["value"].get<std::string>(),
                    .id = value["id"].get<int64_t>(),
                    .type = value["type"].get<std::string>(),
                    .sourceId = value.contains("source_id") && !value["source_id"].is_null()
                        ? std::optional<int32_t>(value["source_id"].get<int32_t>()) : std::nullopt,
                });
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
                    { "albumGain", replayGain.album_gain },
                    { "albumPeak", replayGain.album_peak },
                    { "trackGain", replayGain.track_gain },
                    { "trackPeak", replayGain.track_peak }
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
                mc_replay_gain_values replayGain = MC_REPLAY_GAIN_VALUES_DEFAULT;
                replayGain.album_gain = replayGainJson.value("albumGain", 1.0f);
                replayGain.album_peak = replayGainJson.value("albumPeak", 1.0f);
                replayGain.track_gain = replayGainJson.value("trackGain", 1.0f);
                replayGain.track_peak = replayGainJson.value("trackPeak", 1.0f);
                output->SetReplayGain(replayGain);
            }

            output->SetMetadataState(musik::core::MetadataState::Loaded);
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

        void TrackListFromJson(const nlohmann::json& input, TrackList& output, LibraryBackend::Shared library, bool onlyIds) {
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

        nlohmann::json TrackListToJsonIdList(const TrackList& input) {
            nlohmann::json output;
            for (size_t i = 0; i < input.Count(); i++) {
                output.push_back(input.GetId(i));
            }
            return output;
        }

        nlohmann::json DurationMapToJsonMap(const std::map<size_t, size_t>& input) {
            nlohmann::json output;
            for (auto kv : input) {
                output[std::to_string(kv.first)] = kv.second;
            }
            return output;
        }

        void JsonMapToDuration(const nlohmann::json& input, std::map<size_t, size_t>& output) {
            for (const auto& item : input.items()) {
                output[std::stoi(item.key())] = item.value().get<size_t>();
            }
        }

    }

} } } }
