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

/** @file Auddio.h
 *  @brief Client helpers for the Auddio lyrics/music recognition service.
 *  @details Exposes whether the service is available and a function to asynchronously
 *      look up lyrics for a track. */

#include <functional>
#include <musikcore/library/track/Track.h>

/** @namespace musik::core::auddio
 *  @brief Auddio service client: lyrics lookup. */
namespace musik { namespace core { namespace auddio {
    /** @brief Callback invoked with the lookup result.
     *  @param track The track that was looked up.
     *  @param lyrics The returned lyrics text. */
    using LyricsCallback = std::function<void(musik::core::TrackPtr track, std::string)>;
    /** @return true if the Auddio service is configured and available. */
    extern bool Available();
    /** @brief Asynchronously looks up lyrics for a track.
     *  @param track The track to look up.
     *  @param callback Invoked with the result. */
    extern void FindLyrics(musik::core::TrackPtr track, LyricsCallback callback);
} } }