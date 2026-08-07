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
 * @file PlaybackOverlays.h
 * @brief Factory for overlays that configure playback hardware.
 * @details Provides static helpers that show the output driver chooser, the
 *          output device chooser, the transport type chooser and the replay
 *          gain overlay.
 */

#include <functional>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/sdk/IPlaybackService.h>

namespace musik {
    namespace cube {
        /**
         * @brief Factory for playback-related configuration overlays.
         * @details Each static method shows a modal overlay and invokes the
         *          callback when the user finishes so the caller can reload
         *          or refresh the relevant settings. Not instantiable.
         */
        class PlaybackOverlays {
            public:
                /**
                 * @brief Shows the audio output driver chooser.
                 * @param transportType the transport whose driver is changed
                 * @param callback invoked when the overlay is closed
                 */
                static void ShowOutputDriverOverlay(
                    musik::core::audio::MasterTransport::Type transportType,
                    std::function<void()> callback);

                /**
                 * @brief Shows the output device chooser.
                 * @param callback invoked when the overlay is closed
                 */
                static void ShowOutputDeviceOverlay(std::function<void()> callback);

                /**
                 * @brief Shows the transport type chooser.
                 * @param transportType the currently active transport type
                 * @param callback invoked with the newly selected transport
                 *        type
                 */
                static void ShowTransportOverlay(
                    musik::core::audio::MasterTransport::Type transportType,
                    std::function<void(musik::core::audio::MasterTransport::Type)> callback);

                /**
                 * @brief Shows the replay gain mode chooser.
                 * @param callback invoked when the overlay is closed
                 */
                static void ShowReplayGainOverlay(std::function<void()> callback);

            private:
                PlaybackOverlays();
        };
    }
}
