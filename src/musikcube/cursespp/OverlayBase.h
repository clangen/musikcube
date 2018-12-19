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

#include "Colors.h"
#include "IOverlay.h"
#include "LayoutBase.h"
#include "OverlayStack.h"
#include "TextLabel.h"
#include "Checkbox.h"
#include "TextInput.h"
#include "ListWindow.h"

namespace cursespp {
    class OverlayBase : public LayoutBase, public IOverlay {
        public:
            OverlayBase() : LayoutBase() {
                this->SetFrameVisible(true);
                this->SetFrameColor(CURSESPP_OVERLAY_FRAME);
                this->SetContentColor(CURSESPP_OVERLAY_CONTENT);
            }

            virtual ~OverlayBase() {
                this->stack = nullptr;
            }

            virtual void SetOverlayStack(OverlayStack* stack) {
                this->stack = stack;
            }

            virtual bool IsTop() {
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

        protected:
            static void style(TextLabel& label) {
                label.SetContentColor(CURSESPP_OVERLAY_CONTENT);
            }

            static void style(Checkbox& cb) {
                cb.SetContentColor(CURSESPP_OVERLAY_CONTENT);
                cb.SetFocusedContentColor(CURSESPP_OVERLAY_TEXT_FOCUSED);
            }

            static void style(TextInput& input) {
                if (input.GetStyle() == TextInput::StyleBox) {
                    input.SetFrameColor(CURSESPP_OVERLAY_FRAME);
                    input.SetContentColor(CURSESPP_OVERLAY_CONTENT);
                    input.SetFocusedFrameColor(CURSESPP_OVERLAY_INPUT_FRAME);
                    input.SetFocusedContentColor(CURSESPP_OVERLAY_CONTENT);
                }
                else {
                    input.SetContentColor(CURSESPP_OVERLAY_CONTENT);
                    input.SetFocusedContentColor(CURSESPP_OVERLAY_TEXT_FOCUSED);
                }
            }

            static void style(ListWindow& listWindow, bool frameVisible = false) {
                listWindow.SetContentColor(CURSESPP_OVERLAY_CONTENT);
                listWindow.SetFocusedContentColor(CURSESPP_OVERLAY_CONTENT);
                listWindow.SetFrameColor(CURSESPP_OVERLAY_LIST_FRAME);
                listWindow.SetFocusedFrameColor(CURSESPP_OVERLAY_LIST_FRAME_FOCUSED);
                listWindow.SetFrameVisible(frameVisible);
            }

            OverlayStack* GetOverlayStack() {
                return this->stack;
            }

            void Dismiss() {
                if (this->stack) {
                    stack->Remove(this);
                    this->OnDismissed();
                }
            }

            virtual void OnDismissed() {
                /* for subclass use */
            }

        private:
            OverlayStack* stack;
    };
}
