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
 * @file SettingsOverlays.h
 * @brief Collection of one-off settings and startup dialog overlays.
 * @details Provides free functions that show the locale chooser, the library
 *          type chooser, the first run dialog, the TLS warning dialog and
 *          the transcoder bitrate and format choosers.
 */

#include <functional>

namespace musik { namespace cube {

    /**
     * @brief Overlay helpers used by the settings flow.
     * @details All functions are free functions inside this namespace. The
     *          Check-prefixed functions conditionally show a dialog and do
     *          not take a callback.
     */
    namespace SettingsOverlays {

        /**
         * @brief Shows the locale selection overlay.
         * @param callback invoked when the overlay is closed
         */
        void ShowLocaleOverlay(std::function<void()> callback);

        /**
         * @brief Shows the library type selection overlay.
         * @param callback invoked when the overlay is closed
         */
        void ShowLibraryTypeOverlay(std::function<void()> callback);

        /**
         * @brief Shows the first run dialog if it has not been displayed yet.
         */
        void CheckShowFirstRunDialog();

        /**
         * @brief Shows the TLS warning dialog if it has not been dismissed
         *        yet.
         */
        void CheckShowTlsWarningDialog();

        /**
         * @brief Shows the transcoder bitrate selection overlay.
         * @param callback invoked when the overlay is closed
         */
        void ShowTranscoderBitrateOverlay(std::function<void()> callback);

        /**
         * @brief Shows the transcoder output format selection overlay.
         * @param callback invoked when the overlay is closed
         */
        void ShowTranscoderFormatOverlay(std::function<void()> callback);

    }
} }
