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

/** @file Track.h
 *  @brief Base Track class and TagStore adapter implementing the SDK interfaces.
 *  @details Track is the concrete in-app representation of a music track,
 *      implementing ITrack, IMap and ITagStore semantics. TagStore adapts a Track
 *      to the SDK ITagStore interface for writing. */

#include <musikcore/support/DeleteDefaults.h>
#include <musikcore/sdk/ITagStore.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/sdk/ITrack.h>
#include <atomic>
#include <vector>
#include <map>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    class Track;
    typedef std::shared_ptr<Track> TrackPtr; /**< Shared track alias. */

    /** @brief Base class for all in-app track representations.
     *  @details Holds metadata as a multimap of key/value strings. Reads go
     *      through the IMap/ITrack interfaces; writes use the ITagStore methods
     *      (SetValue, SetThumbnail, SetReplayGain). Concrete subclasses (LibraryTrack,
     *      IndexerTrack) define how metadata is loaded and persisted. */
    class Track :
        public musik::core::sdk::ITrack,
        public std::enable_shared_from_this<Track>
    {
        public:
            typedef std::multimap<std::string, std::string> MetadataMap; /**< Key/value metadata store. */
            typedef std::pair<MetadataMap::iterator, MetadataMap::iterator> MetadataIteratorRange; /**< Range over values for one key. */

            /** @return The library this track belongs to, or nullptr. */
            virtual musik::core::ILibraryPtr Library() noexcept;
            /** @return The id of the owning library, or -1. */
            virtual int LibraryId() noexcept;

            /* ITrack is a ready only interface; we use the ITagStore interface
            for writing. we replicate the interface here, and have TagStore pass
            through to us */
            /** @brief Sets a metadata value.
             *  @param key The metadata key.
             *  @param value The value to store. */
            virtual void SetValue(const char* key, const char* value) = 0;
            /** @brief Removes all values for a metadata key.
             *  @param key The metadata key. */
            virtual void ClearValue(const char* key) = 0;
            /** @brief Sets the embedded thumbnail data.
             *  @param data Raw thumbnail bytes.
             *  @param size Size of the data. */
            virtual void SetThumbnail(const char* data, long size) = 0;
            /** @return true if the given key has a value.
             *  @param key The metadata key. */
            virtual bool Contains(const char* key) = 0;
            /** @return true if a thumbnail is present. */
            virtual bool ContainsThumbnail() = 0;
            /** @brief Sets the ReplayGain profile.
             *  @param replayGain The gain values. */
            virtual void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) = 0;

            /* IResource */
            /** @return The track id. */
            int64_t GetId() override;
            /** @return IResource::Class::Map. */
            Class GetClass() override;
            /** @return The resource type string ("track"). */
            const char* GetType() override;

            /* IValue */
            /** @brief Copies the track title into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied. */
            size_t GetValue(char* dst, size_t size) override;

            /* IMap */
            /** @brief No-op; tracks are shared and not manually freed. */
            void Release() noexcept override;
            /** @brief Copies a string value into the destination buffer.
             *  @param key The metadata key.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied, or -1 if the key is missing. */
            int GetString(const char* key, char* dst, int size) override = 0;
            /** @return The 64-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            long long GetInt64(const char* key, long long defaultValue = 0LL) override = 0;
            /** @return The 32-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            int GetInt32(const char* key, unsigned int defaultValue = 0) override = 0;
            /** @return The double value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            double GetDouble(const char* key, double defaultValue = 0.0f) override = 0;

            /* ITrack */
            /** @brief Retains a reference (no-op for shared tracks). */
            void Retain() noexcept override;
            /** @brief Copies the URI into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied. */
            int Uri(char* dst, int size) override = 0;

            /* implementation specific */
            /** @brief Sets the track id.
             *  @param id The new id. */
            virtual void SetId(int64_t id) = 0;
            /** @return The value of the given key as a string.
             *  @param metakey The metadata key. */
            virtual std::string GetString(const char* metakey) = 0;
            /** @return The URI of the track. */
            virtual std::string Uri() = 0;
            /** @return An iterator range over the values of the given key.
             *  @param metakey The metadata key. */
            virtual MetadataIteratorRange GetValues(const char* metakey) = 0;
            /** @return An iterator range over all metadata. */
            virtual MetadataIteratorRange GetAllValues() = 0;
            /** @return A deep copy of this track. */
            virtual TrackPtr Copy() = 0;
            /** @brief Sets the metadata state.
             *  @param state The new state. */
            virtual void SetMetadataState(musik::core::sdk::MetadataState state) = 0;

            /* for SDK interop */
            /** @return This track as a raw SDK ITrack pointer (borrowed). */
            ITrack* GetSdkValue();
    };

    /** @brief Adapts a Track to the SDK ITagStore interface for writing.
     *  @details Reference-counted wrapper that forwards ITagStore calls to the
     *      wrapped Track. */
    class TagStore : public musik::core::sdk::ITagStore {
        public:
            DELETE_CLASS_DEFAULTS(TagStore)

            /** @brief Wraps a shared track.
             *  @param track The track to wrap. */
            TagStore(TrackPtr track) noexcept;
            /** @brief Wraps a track by reference.
             *  @param track The track to wrap. */
            TagStore(Track& track);

            virtual ~TagStore() noexcept { }

            /** @brief Casts the wrapped track to a concrete type.
             *  @tparam T The destination type.
             *  @return The casted track, or nullptr if the cast fails. */
            template <typename T> T As() {
                return dynamic_cast<T>(track.get());
            }

            /** @brief Retains a reference to the wrapper. */
            void Retain() noexcept override;
            /** @brief Releases a reference; frees the wrapper at zero. */
            void Release() noexcept override;
            /** @brief Sets a metadata value on the wrapped track.
             *  @param key The metadata key.
             *  @param value The value to store. */
            void SetValue(const char* key, const char* value) override;
            /** @brief Removes a metadata value from the wrapped track.
             *  @param key The metadata key. */
            void ClearValue(const char* key) override;
            /** @return true if the given key has a value.
             *  @param key The metadata key. */
            bool Contains(const char* key) override;
            /** @return true if the wrapped track has a thumbnail. */
            bool ContainsThumbnail() override;
            /** @brief Sets the thumbnail on the wrapped track.
             *  @param data Raw thumbnail bytes.
             *  @param size Size of the data. */
            void SetThumbnail(const char* data, long size) override;
            /** @brief Sets the ReplayGain on the wrapped track.
             *  @param replayGain The gain values. */
            void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

        private:
            TrackPtr track; /**< Wrapped track. */
            std::atomic<int> count; /**< Reference count. */
    };

} }
