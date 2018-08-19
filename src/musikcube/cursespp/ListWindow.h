//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "IScrollable.h"
#include "IScrollAdapter.h"
#include "ScrollableWindow.h"
#include <sigslot/sigslot.h>

namespace cursespp {
    class ListWindow :
        public ScrollableWindow
    #if (__clang_major__ == 7 && __clang_minor__ == 3)
        , public std::enable_shared_from_this<ListWindow>
    #endif
     {
        public:
            static size_t NO_SELECTION;

            sigslot::signal3<ListWindow*, size_t, size_t> SelectionChanged;
            sigslot::signal2<ListWindow*, size_t> Invalidated;
            sigslot::signal2<ListWindow*, size_t> EntryActivated;
            sigslot::signal2<ListWindow*, size_t> EntryContextMenu;

            ListWindow(std::shared_ptr<IScrollAdapter> adapter, IWindow *parent = nullptr);
            ListWindow(IWindow *parent = nullptr);

            virtual ~ListWindow();

            virtual void ScrollToTop();
            virtual void ScrollToBottom();
            virtual void ScrollUp(int delta = 1);
            virtual void ScrollDown(int delta = 1);
            virtual void PageUp();
            virtual void PageDown();
            virtual void ScrollTo(size_t index);

            virtual size_t GetSelectedIndex();
            virtual void SetSelectedIndex(size_t index);
            virtual bool IsEntryVisible(size_t index);
            virtual void Invalidate();
            virtual void OnAdapterChanged();

            virtual const IScrollAdapter::ScrollPosition& GetScrollPosition();

            virtual bool KeyPress(const std::string& key);
            virtual bool MouseEvent(const IMouseHandler::Event& event);

            void SetScrollbarVisible(bool visible);

        protected:
            virtual void OnSelectionChanged(size_t newIndex, size_t oldIndex);
            virtual void OnEntryActivated(size_t index);
            virtual void OnEntryContextMenu(size_t index);
            virtual void OnInvalidated();
            virtual void OnDimensionsChanged();
            virtual void DecorateFrame();
            virtual IScrollAdapter::ScrollPosition& GetMutableScrollPosition();

        private:
            virtual bool IsSelectedItemCompletelyVisible();

            bool showScrollbar;
            IScrollAdapter::ScrollPosition scrollPosition;
            size_t selectedIndex;
    };
}
