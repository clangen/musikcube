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
 * @file HotkeysAdapter.h
 * @brief Scroll adapter that lists the configured hotkey bindings.
 * @details Provides the list of hotkey mapping entries to a ListWindow for
 *          the hotkeys layout.
 */

#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/ScrollableWindow.h>

namespace musik {
    namespace cube {
        /**
         * @brief Scroll adapter for the hotkey bindings list.
         * @details Feeds the configured hotkey mappings to a ListWindow; each
         *          entry renders a hotkey action and its bound key sequence.
         */
        class HotkeysAdapter : public cursespp::ScrollAdapterBase {
            public:
                /** @brief sentinel index used when no entry is selected */
                static const size_t NO_INDEX = (size_t)-1;

                /**
                 * @brief Creates an empty adapter.
                 */
                HotkeysAdapter();
                /**
                 * @brief Destroys the adapter.
                 */
                virtual ~HotkeysAdapter();

                /* ScrollAdapterBase */
                /**
                 * @brief Returns the number of hotkey bindings.
                 * @return the entry count
                 */
                size_t GetEntryCount() override;
                /**
                 * @brief Returns the hotkey entry at the given index.
                 * @param window the scrollable window requesting the entry
                 * @param index the entry index
                 * @return the entry, or an empty entry
                 */
                EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;
        };
    }
}
