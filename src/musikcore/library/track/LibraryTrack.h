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

/** @file LibraryTrack.h
 *  @brief Track implementation representing a track in a library.
 *  @details A read-mostly Track bound to a specific library. Metadata is loaded
 *      on demand and cached; the track also stores its ReplayGain and metadata
 *      state. */

#include <musikcore/config.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/db/Connection.h>
#include <mutex>
#include <atomic>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief A Track stored in and loaded from a library.
     *  @details Identified by a track id and a library id. Reads resolve lazily
     *      against the owning library's database; cached metadata lives in an
     *      internal MetadataMap. */
    class LibraryTrack: public Track {
        public:
            /** @brief Creates an empty library track. */
            LibraryTrack() noexcept;
            /** @brief Creates a track identified by id and library id.
             *  @param id The track id.
             *  @param libraryId The owning library id. */
            LibraryTrack(int64_t id, int libraryId);
            /** @brief Creates a track bound to a library.
             *  @param id The track id.
             *  @param library The owning library. */
            LibraryTrack(int64_t id, musik::core::ILibraryPtr library);
            virtual ~LibraryTrack();

            /** @return The owning library id. */
            int LibraryId() noexcept override;

            /** @return The track id. */
            int64_t GetId() noexcept override;
            /** @brief Sets the track id.
             *  @param id The new id. */
            void SetId(int64_t id) noexcept override { this->id = id; }

            /** @return The value of the given key as a string.
             *  @param metakey The metadata key. */
            std::string GetString(const char* metakey) override;
            /** @return The URI of the track. */
            std::string Uri() override;

            /* ITagStore */
            /** @brief Sets a metadata value.
             *  @param metakey The metadata key.
             *  @param value The value to store. */
            void SetValue(const char* metakey, const char* value) override;
            /** @brief Removes a metadata value.
             *  @param metakey The metadata key. */
            void ClearValue(const char* metakey) override;
            /** @return true if the given key has a value.
             *  @param metakey The metadata key. */
            bool Contains(const char* metakey) override;
            /** @brief Sets the embedded thumbnail data.
             *  @param data Raw thumbnail bytes.
             *  @param size Size of the data. */
            void SetThumbnail(const char* data, long size) override;
            /** @return true if a thumbnail is present. */
            bool ContainsThumbnail() override;
            /** @brief Sets the ReplayGain profile.
             *  @param replayGain The gain values. */
            void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

            /* ITrack */
            /** @return The 64-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            long long GetInt64(const char* key, long long defaultValue = 0LL) override;
            /** @return The 32-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            int GetInt32(const char* key, unsigned int defaultValue = 0) override;
            /** @return The double value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            double GetDouble(const char* key, double defaultValue = 0.0f) override;
            /** @brief Copies a string value into the destination buffer.
             *  @param key The metadata key.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst. */
            int GetString(const char* key, char* dst, int size) override;
            /** @brief Copies the URI into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst. */
            int Uri(char* dst, int size) override;
            /** @return The stored ReplayGain profile. */
            musik::core::sdk::ReplayGain GetReplayGain() noexcept override;
            /** @return The current metadata state. */
            musik::core::sdk::MetadataState GetMetadataState() noexcept override;
            /** @brief Sets the metadata state.
             *  @param state The new state. */
            void SetMetadataState(musik::core::sdk::MetadataState state) noexcept override;

            /** @return An iterator range over the values of the given key.
             *  @param metakey The metadata key. */
            MetadataIteratorRange GetValues(const char* metakey) override;
            /** @return An iterator range over all metadata. */
            MetadataIteratorRange GetAllValues() noexcept override;
            /** @return A deep copy of this track. */
            TrackPtr Copy() override;

        private:
            int64_t id; /**< Track id. */
            int libraryId; /**< Owning library id. */
            Track::MetadataMap metadata; /**< Cached metadata. */
            std::mutex mutex; /**< Guards metadata access. */
            std::atomic<musik::core::sdk::MetadataState> state; /**< Metadata load state. */
            musik::core::sdk::ReplayGain* gain; /**< Cached ReplayGain, or nullptr. */
    };

} }
