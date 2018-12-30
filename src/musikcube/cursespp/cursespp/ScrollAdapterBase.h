//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/IScrollAdapter.h>
#include <functional>
#include <deque>

namespace cursespp {
    class ScrollAdapterBase : public IScrollAdapter {
        public:
            typedef std::function<Color(
                ScrollableWindow*,
                size_t,
                size_t,
                EntryPtr)> ItemDecorator;

            ScrollAdapterBase();
            virtual ~ScrollAdapterBase();

            virtual void SetDisplaySize(size_t width, size_t height);
            virtual size_t GetLineCount();

            virtual void DrawPage(
                ScrollableWindow* window,
                size_t index,
                ScrollPosition& result);

            virtual size_t GetEntryCount() = 0;
            virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) = 0;

            virtual void SetItemDecorator(ItemDecorator decorator) { this->decorator = decorator; }

        protected:
            size_t GetVisibleItems(
                cursespp::ScrollableWindow* window,
                size_t desiredTopIndex,
                std::deque<EntryPtr>& target);

            virtual ItemDecorator GetItemDecorator() { return this->decorator; }

            size_t GetWidth() { return this->width; }
            size_t GetHeight() { return this->height; }

        private:
            size_t width, height;
            ItemDecorator decorator;
    };
}
