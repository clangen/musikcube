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

#include <cursespp/OverlayBase.h>
#include <cursespp/ListWindow.h>

#include <vector>
#include <map>

namespace cursespp {
    class ListOverlay:
        public OverlayBase,
        public sigslot::has_slots<>
    {
        public:
            using ItemSelectedCallback = std::function<void(ListOverlay* sender, IScrollAdapterPtr adapter, size_t index)>;
            using DeleteKeyCallback = std::function<void(ListOverlay* sender, IScrollAdapterPtr adapter, size_t index)>;
            using DismissedCallback = std::function<void(ListOverlay* sender)>;
            using KeyInterceptorCallback = std::function<bool(ListOverlay* sender, std::string key)>;

            ListOverlay();
            virtual ~ListOverlay();

            ListOverlay& SetTitle(const std::string& title);
            ListOverlay& SetAdapter(IScrollAdapterPtr adapter);
            ListOverlay& SetItemSelectedCallback(ItemSelectedCallback cb);
            ListOverlay& SetDeleteKeyCallback(DeleteKeyCallback cb);
            ListOverlay& SetDismissedCallback(DismissedCallback cb);
            ListOverlay& SetKeyInterceptorCallback(KeyInterceptorCallback cb);
            ListOverlay& SetSelectedIndex(size_t index);
            ListOverlay& SetWidth(int width);
            ListOverlay& SetWidthPercent(int percent);
            ListOverlay& SetAutoDismiss(bool autoDismiss);

            size_t GetSelectedIndex();

            virtual void Layout() override;
            virtual bool KeyPress(const std::string& key) override;

            void RefreshAdapter();

        protected:
            virtual void OnVisibilityChanged(bool visible) override;
            virtual void OnDismissed() override;

        private:
            void OnListEntryActivated(cursespp::ListWindow* sender, size_t index);

            class CustomListWindow;

            void RecalculateSize();
            bool ScrollbarVisible();
            void UpdateContents();

            std::string title;
            int x, y;
            int width, height;
            int setWidth, setWidthPercent;
            bool autoDismiss;
            IScrollAdapterPtr adapter;
            std::shared_ptr<CustomListWindow> listWindow;
            std::shared_ptr<Window> scrollbar;
            ItemSelectedCallback itemSelectedCallback;
            DeleteKeyCallback deleteKeyCallback;
            DismissedCallback dismissedCallback;
            KeyInterceptorCallback keyInterceptorCallback;
    };
}