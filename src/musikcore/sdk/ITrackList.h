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

/** @file ITrackList.h @brief Defines the ITrackList interface, a read-only collection of tracks. */
#pragma once

#include <stddef.h>
#include "ITrack.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A read-only, indexable collection of tracks. */
    class ITrackList {
        public:
            /** @brief Releases the list; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns the number of tracks in the list.
             *  @return The track count. */
            virtual size_t Count() const = 0;

            /** @brief Returns the id of the track at the given index.
             *  @param index The zero-based index.
             *  @return The track id. */
            virtual int64_t GetId(size_t index) const = 0;

            /** @brief Returns the index of the track with the given id.
             *  @param id The track id to find.
             *  @return The index, or -1 if not found. */
            virtual int IndexOf(int64_t id) const = 0;

            /** @brief Returns the track at the given index.
             *  @param index The zero-based index.
             *  @return The track, or null if out of range. */
            virtual ITrack* GetTrack(size_t index) const = 0;
    };

} } }

