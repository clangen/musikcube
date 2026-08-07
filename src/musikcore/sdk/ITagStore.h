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

/** @file ITagStore.h @brief Defines the ITagStore interface for storing track metadata and thumbnails. */
#pragma once

#include "ReplayGain.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A mutable collection of metadata tags and associated data for a
     *  single track, used when reading and writing tags. */
    class ITagStore {
        public:
            /** @brief Increments the reference count of the store. */
            virtual void Retain() = 0;

            /** @brief Releases the store; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Sets a metadata value.
             *  @param key The metadata key.
             *  @param value The value to store. */
            virtual void SetValue(const char* key, const char* value) = 0;

            /** @brief Removes a metadata value.
             *  @param key The metadata key to clear. */
            virtual void ClearValue(const char* key) = 0;

            /** @brief Returns whether the store contains the given key.
             *  @param key The metadata key to check.
             *  @return True if the key exists. */
            virtual bool Contains(const char* key) = 0;

            /** @brief Sets the thumbnail image data for the track.
             *  @param data The raw thumbnail data.
             *  @param size The size of the thumbnail data, in bytes. */
            virtual void SetThumbnail(const char *data, long size) = 0;

            /** @brief Returns whether the store contains thumbnail data.
             *  @return True if a thumbnail is present. */
            virtual bool ContainsThumbnail() = 0;

            /** @brief Sets the ReplayGain values for the track.
             *  @param replayGain The ReplayGain values to store. */
            virtual void SetReplayGain(const ReplayGain& replayGain) = 0;
    };

} } }
