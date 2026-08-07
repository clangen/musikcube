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

/** @file IScrollable.h @brief Interface for windows that support scrolling. */
#pragma once

namespace cursespp {
    /** @brief Contract for widgets that can scroll their content.
     *
     *  @details Implementors expose a scrollable viewport over a larger set of
     *  content. Callers can jump to the top or bottom, scroll by an arbitrary
     *  number of entries, or page up/down by one viewport. ScrollableWindow
     *  implements this interface to provide arrow and page-key navigation over
     *  an IScrollAdapter.
     */
    class IScrollable {
        public:
            virtual ~IScrollable() { }

            /** @brief Scrolls to the very beginning of the content. */
            virtual void ScrollToTop() = 0;
            /** @brief Scrolls to the very end of the content. */
            virtual void ScrollToBottom() = 0;
            /** @brief Scrolls upward by a number of entries.
             *  @param delta the number of entries to scroll.
             */
            virtual void ScrollUp(int delta = 1) = 0;
            /** @brief Scrolls downward by a number of entries.
             *  @param delta the number of entries to scroll.
             */
            virtual void ScrollDown(int delta = 1) = 0;
            /** @brief Scrolls up by one viewport page. */
            virtual void PageUp() = 0;
            /** @brief Scrolls down by one viewport page. */
            virtual void PageDown() = 0;
    };
}
