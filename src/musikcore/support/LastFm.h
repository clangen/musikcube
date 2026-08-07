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

/** @file LastFm.h
 *  @brief Last.fm scrobbling client.
 *  @details Handles account linking (token + session creation) and scrobbling of
 *      played tracks, plus persistence of the authenticated session. */

#include <musikcore/library/track/Track.h>

/** @namespace musik::core::lastfm
 *  @brief Last.fm scrobbling and authentication helpers. */
namespace musik { namespace core { namespace lastfm {
    /** @brief An authenticated Last.fm session. */
    struct Session {
        bool valid{ false }; /**< Whether the session is valid. */
        std::string username, token, sessionId; /**< Account and session identifiers. */
    };

    using TokenCallback = std::function<void(std::string)>; /**< Callback receiving an auth token. */
    using SessionCallback = std::function<void(Session)>; /**< Callback receiving a session. */

    /** @brief Requests an account-link token.
     *  @param callback Invoked with the token. */
    void CreateAccountLinkToken(TokenCallback callback);
    /** @brief Builds the browser URL for linking the account.
     *  @param token The account-link token.
     *  @return The authorization URL. */
    const std::string CreateAccountLinkUrl(const std::string& token);
    /** @brief Exchanges a token for an authenticated session.
     *  @param token The account-link token.
     *  @param session Callback invoked with the resulting session. */
    void CreateSession(const std::string& token, SessionCallback session);
    /** @brief Scrobbles a track (recorded after it is fully played).
     *  @param track The track to scrobble. */
    void Scrobble(musik::core::TrackPtr track);
    /** @brief Reports the currently playing track.
     *  @param track The track being played. */
    void UpdateNowPlaying(musik::core::TrackPtr track);

    /** @return The persisted session, if any. */
    Session LoadSession();
    /** @brief Persists a session.
     *  @param session The session to save. */
    void SaveSession(const Session& session);
    /** @brief Clears the persisted session. */
    void ClearSession();
} } }

