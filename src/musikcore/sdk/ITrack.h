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

/** @file ITrack.h @brief Defines the ITrack interface for accessing track metadata and playback data. */
#pragma once

#include "IMap.h"
#include "ReplayGain.h"
#include "constants.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief Represents a single track in the library, exposing its metadata
     *  as key/value pairs as well as playback-relevant data. */
    class ITrack: public IMap {
        public:
            /** @brief Increments the reference count of the track. */
            virtual void Retain() = 0;

            /** @brief Retrieves the URI of the track.
             *  @param dst The destination buffer for the URI.
             *  @param size The capacity of the destination buffer.
             *  @return The number of bytes written, or the required size if the buffer was too small. */
            virtual int Uri(char* dst, int size) = 0;

            /* sdk v19 */
            /** @brief Returns the ReplayGain values of the track.
             *  @return The ReplayGain values. */
            virtual ReplayGain GetReplayGain() = 0;

            /** @brief Returns the metadata loading state of the track.
             *  @return The metadata state. */
            virtual MetadataState GetMetadataState() = 0;
    };

} } }

