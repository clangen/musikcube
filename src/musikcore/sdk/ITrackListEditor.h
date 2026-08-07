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

/** @file ITrackListEditor.h @brief Defines the ITrackListEditor interface for mutating a track list. */
#pragma once

#include <stddef.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A mutable editor over a track list, supporting insertion,
     *  removal, reordering, and shuffling of tracks. */
    class ITrackListEditor {
        public:
            /** @brief Inserts a track at the given index.
             *  @param id The id of the track to insert.
             *  @param index The zero-based index at which to insert.
             *  @return True if the track was inserted. */
            virtual bool Insert(int64_t id, size_t index) = 0;

            /** @brief Swaps the positions of two tracks.
             *  @param index1 The index of the first track.
             *  @param index2 The index of the second track.
             *  @return True if the tracks were swapped. */
            virtual bool Swap(size_t index1, size_t index2) = 0;

            /** @brief Moves a track from one position to another.
             *  @param from The current index of the track.
             *  @param to The destination index.
             *  @return True if the track was moved. */
            virtual bool Move(size_t from, size_t to) = 0;

            /** @brief Removes the track at the given index.
             *  @param index The zero-based index of the track to remove.
             *  @return True if the track was removed. */
            virtual bool Delete(size_t index) = 0;

            /** @brief Appends a track to the end of the list.
             *  @param id The id of the track to add. */
            virtual void Add(const int64_t id) = 0;

            /** @brief Removes all tracks from the list. */
            virtual void Clear() = 0;

            /** @brief Randomizes the order of the tracks in the list. */
            virtual void Shuffle() = 0;

            /** @brief Releases the editor; callers must invoke this when done. */
            virtual void Release() = 0;
    };

} } }

