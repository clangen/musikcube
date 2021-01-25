//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <cursespp/ILayout.h>
#include <cursespp/Window.h>

#include <sigslot/sigslot.h>
#include <vector>

namespace cursespp {
    class LayoutBase:
        public Window,
        public ILayout
    {
        public:
            enum FocusDirection {
                FocusForward,
                FocusBackward
            };

            sigslot::signal1<FocusDirection> FocusTerminated;
            sigslot::signal1<FocusDirection> FocusWrapped;

            LayoutBase(IWindow* parent = NULL);
            ~LayoutBase() override;

            /* IWindow */
            void Show() override;
            void Hide() override;
            void Invalidate() override;
            void OnParentVisibilityChanged(bool visible) override;
            void OnChildVisibilityChanged(bool visible, IWindow* child) override;
            void Focus() override;

            /* IOrderable */
            void BringToTop() override;
            void SendToBottom() override;

            /* ILayout */
            IWindowPtr FocusNext() override;
            IWindowPtr FocusPrev() override;

            IWindowPtr GetFocus() override;
            bool SetFocus(IWindowPtr window) override;

            int GetFocusIndex() override;
            void SetFocusIndex(int index, bool applyFocus = true) override;

            int GetFocusableCount() override;
            IWindowPtr GetFocusableAt(int index) override;

            FocusMode GetFocusMode() const override;
            void SetFocusMode(FocusMode mode) override;

            IWindowPtr FocusFirst() override;
            IWindowPtr FocusLast() override;

            void OnVisibilityChanged(bool visible) override;

            void Layout() override;

            /* IKeyHandler */
            bool KeyPress(const std::string& key) override;

            /* IMouseHandler */
            bool MouseEvent(const IMouseHandler::Event& mouseEvent) override;

            /* IWindowGroup */
            bool AddWindow(IWindowPtr window) override;
            bool RemoveWindow(IWindowPtr window) override;
            size_t GetWindowCount() override;
            IWindowPtr GetWindowAt(size_t position) override;

            /* virtual methods we define */
            virtual void OnLayout();

        protected:

            IWindowPtr EnsureValidFocus();

            /* virtual methods we define */
            virtual IWindowPtr EnsureValidFocusFromNext();
            virtual IWindowPtr EnsureValidFocusFromPrev();

        private:
            void AddFocusable(IWindowPtr window);
            void RemoveFocusable(IWindowPtr window);
            void SortFocusables();
            void IndexFocusables();
            LayoutBase* GetLayoutAtFocusIndex();

            std::vector<IWindowPtr> children;
            std::vector<IWindowPtr> focusable;
            int focused;
            FocusMode focusMode;
    };
}
