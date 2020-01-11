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

#include <cursespp/IOverlay.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/OverlayStack.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/Checkbox.h>
#include <cursespp/Colors.h>
#include <cursespp/ListWindow.h>

namespace cursespp {
    class OverlayBase : public LayoutBase, public IOverlay {
        public:
            OverlayBase() : LayoutBase() {
                this->SetFrameVisible(true);
                this->SetFrameColor(Color::OverlayFrame);
                this->SetContentColor(Color::OverlayContent);
            }

            virtual ~OverlayBase() {
                this->stack = nullptr;
            }

            virtual void SetOverlayStack(OverlayStack* stack) override {
                this->stack = stack;
            }

            virtual bool IsTop() override {
                if (LayoutBase::IsTop()) {
                    return true;
                }

                for (size_t i = 0; i < this->GetWindowCount(); i++) {
                    if (this->GetWindowAt(i)->IsTop()) {
                        return true;
                    }
                }

                return false;
            }

            virtual IWindowPtr FocusFirst() override {
                auto focus = LayoutBase::FocusFirst();
                if (!focus) {
                    focus = shared_from_this();
                    this->Focus();
                }
                return focus;
            }

            void Dismiss() {
                if (this->stack) {
                    stack->Remove(this);
                    this->OnDismissed();
                }
            }

        protected:
            static void style(TextLabel& label) {
                label.SetContentColor(Color::OverlayContent);
                label.SetFocusedContentColor(Color::OverlayTextFocused);
            }

            static void style(Checkbox& cb) {
                cb.SetContentColor(Color::OverlayContent);
                cb.SetFocusedContentColor(Color::OverlayTextFocused);
            }

            static void style(TextInput& input) {
                if (input.GetStyle() == TextInput::StyleBox) {
                    input.SetFrameColor(Color::OverlayFrame);
                    input.SetContentColor(Color::OverlayContent);
                    input.SetFocusedFrameColor(Color::OverlayTextInputFrame);
                    input.SetFocusedContentColor(Color::OverlayContent);
                }
                else {
                    input.SetContentColor(Color::OverlayContent);
                    input.SetFocusedContentColor(Color::OverlayTextFocused);
                }
            }

            static void style(ListWindow& listWindow, bool frameVisible = false) {
                listWindow.SetContentColor(Color::OverlayContent);
                listWindow.SetFocusedContentColor(Color::OverlayContent);
                listWindow.SetFrameColor(Color::OverlayListFrame);
                listWindow.SetFocusedFrameColor(Color::OverlayListFrameFocused);
                listWindow.SetFrameVisible(frameVisible);
            }

            OverlayStack* GetOverlayStack() {
                return this->stack;
            }

            virtual void OnDismissed() {
                /* for subclass use */
            }

        private:
            OverlayStack* stack;
    };
}
