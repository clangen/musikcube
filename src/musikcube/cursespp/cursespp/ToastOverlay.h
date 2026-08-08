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

/** @file ToastOverlay.h @brief A transient notification overlay that auto-dismisses. */
#pragma once

#include <cursespp/OverlayBase.h>
#include <vector>

namespace cursespp {
    /** @brief A modal overlay that shows a transient message and dismisses itself.
     *
     *  @details ToastOverlay displays a small framed box with a word-wrapped
     *  message. It is shown for a fixed duration (default 3 seconds); while
     *  visible it consumes all input. A runtime timer message triggers the
     *  auto-dismissal when the duration elapses. It is created and shown
     *  exclusively through the static Show() factory.
     */
    class ToastOverlay:
        public OverlayBase,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Shows a transient message overlay.
             *  @param text the message to display.
             *  @param durationMs how long the toast remains visible (ms).
             */
            static void Show(const std::string& text, int durationMs = 3000);

            /** @brief Destroys the toast. */
            virtual ~ToastOverlay();

            /** @brief Non-copyable. */
            ToastOverlay(const ToastOverlay& other) = delete;
            /** @brief Non-assignable. */
            ToastOverlay& operator=(const ToastOverlay& other) = delete;

            /* IWindow */
            /** @brief Arranges the toast and its contents. */
            void Layout() override;
            /** @brief Consumes all keys so the toast is modal while visible.
             *  @param key the normalized key string.
             *  @return always true (keys are swallowed).
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Handles the timer message that triggers auto-dismissal.
             *  @param message the incoming runtime message.
             */
            void ProcessMessage(musik::core::runtime::IMessage& message) override;
            /** @brief Renders the toast contents. */
            void OnRedraw() override;

        protected:
            /** @brief Starts the dismissal timer when the toast becomes visible.
             *  @param visible the new visible state.
             */
            void OnVisibilityChanged(bool visible) override;

        private:
            /** @brief Creates a toast with a fixed duration.
             *  @param text the message to display.
             *  @param durationMs how long the toast stays visible (ms).
             */
            ToastOverlay(const std::string& text, long durationMs);

            void RecalculateSize();

            bool ticking;                /**< Whether the dismissal timer is running. */
            std::string title;           /**< The toast message text. */
            std::vector<std::string> titleLines; /**< Word-wrapped message lines. */
            int durationMs;              /**< The display duration in milliseconds. */
            int x, y;                    /**< The toast position. */
            int width, height;           /**< The toast dimensions. */
    };
}