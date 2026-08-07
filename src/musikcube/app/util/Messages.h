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

/**
 * @file Messages.h
 * @brief Application-specific message identifiers.
 * @details Declares the runtime message ids used by the UI layer, offset
 *          past the core user messages so the two ranges cannot collide.
 *          The category sub-namespace maps the category types to small ids.
 */

#include <stdafx.h>
#include <musikcore/support/Messages.h>

namespace musik {
    namespace cube {
        namespace message {
            /**
             * @brief First application-specific message id.
             */
            static const int First = musik::core::message::User + 1024;

            /**
             * @brief Category type ids carried in category messages.
             */
            namespace category { /* User1 */
                static const int Album = 0;        /**< album category */
                static const int AlbumArtist = 1;  /**< album artist category */
                static const int Artist = 2;       /**< artist category */
                static const int Genre = 3;        /**< genre category */
            }

            static const int JumpToCategory         = First + 1;  /**< jump to a library category */
            static const int IndexerStarted         = First + 2;  /**< the indexer started */
            static const int IndexerProgress        = First + 3;  /**< indexer progress update */
            static const int IndexerFinished        = First + 4;  /**< the indexer finished */
            static const int RequeryTrackList       = First + 5;  /**< requery the track list */
            static const int RequeryCategoryList    = First + 6;  /**< requery the category list */
            static const int RefreshTransport       = First + 7;  /**< refresh the transport display */
            static const int TransportBuffering     = First + 8;  /**< transport buffering state changed */
            static const int RefreshLogs            = First + 9;  /**< refresh the console log view */
            static const int UpdateCheckFinished    = First + 10; /**< the update check completed */
            static const int JumpToConsole          = First + 11; /**< navigate to the console */
            static const int JumpToLibrary          = First + 12; /**< navigate to the library */
            static const int JumpToSettings         = First + 13; /**< navigate to the settings */
            static const int JumpToLyrics           = First + 14; /**< navigate to the lyrics */
            static const int JumpToHotkeys          = First + 15; /**< navigate to the hotkeys */
            static const int JumpToPlayQueue        = First + 16; /**< navigate to the play queue */
            static const int SetLastFmState         = First + 17; /**< update the Last.fm state */
            static const int UpdateEqualizer        = First + 18; /**< update the equalizer display */
            static const int DebugLog               = First + 19; /**< a debug log entry was written */
            static const int LyricsLoaded           = First + 20; /**< lyrics were loaded */
            static const int FocusBrowseFilter      = First + 21; /**< focus the browse category filter */

        }
    }
}
