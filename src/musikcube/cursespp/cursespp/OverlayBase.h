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

/** @file OverlayBase.h @brief Base class for modal overlays layered over the main layout. */
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
    /** @brief A LayoutBase that participates in the application's OverlayStack.
     *
     *  @details OverlayBase combines a LayoutBase (so it can host child
     *  windows) with the IOverlay contract. Overlays are modal: while visible
     *  they sit on top of the main layout and consume all input. The overlay is
     *  registered with an OverlayStack by the framework; Dismiss() removes it
     *  from that stack and fires OnDismissed(). The default constructor applies
     *  the overlay color palette and frame visibility. A set of static style()
     *  helpers apply the shared overlay colors to TextLabel, Checkbox,
     *  TextInput and ListWindow children.
     */
    class OverlayBase : public LayoutBase, public IOverlay {
        public:
            /** @brief Creates an overlay with a visible frame and overlay colors. */
            OverlayBase() : LayoutBase() {
                this->SetFrameVisible(true);
                this->SetFrameColor(Color::OverlayFrame);
                this->SetContentColor(Color::OverlayContent);
            }

            /** @brief Destroys the overlay, detaching it from its stack. */
            virtual ~OverlayBase() {
                this->stack = nullptr;
            }

            /** @brief Associates this overlay with its owning stack.
             *  @param stack the OverlayStack that manages this overlay.
             */
            void SetOverlayStack(OverlayStack* stack) override {
                this->stack = stack;
            }

            /** @brief Returns whether this overlay is the top-most window in the stack.
             *  @return true if the overlay (or a child) is on top.
             */
            bool IsTop() override {
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

            /** @brief Removes this overlay from its stack and fires OnDismissed(). */
            void Dismiss() {
                if (this->stack) {
                    stack->Remove(this);
                    this->OnDismissed();
                }
            }

        protected:
            /** @brief Applies the shared overlay colors to a TextLabel.
             *  @param label the label to style.
             */
            static void style(TextLabel& label) {
                label.SetContentColor(Color::OverlayContent);
                label.SetFocusedContentColor(Color::OverlayTextFocused);
            }

            /** @brief Applies the shared overlay colors to a Checkbox.
             *  @param cb the checkbox to style.
             */
            static void style(Checkbox& cb) {
                cb.SetContentColor(Color::OverlayContent);
                cb.SetFocusedContentColor(Color::OverlayTextFocused);
            }

            /** @brief Applies the shared overlay colors to a TextInput.
             *  @param input the input to style.
             */
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

            /** @brief Applies the shared overlay colors to a ListWindow.
             *  @param listWindow the list to style.
             *  @param frameVisible whether the list frame is drawn.
             */
            static void style(ListWindow& listWindow, bool frameVisible = false) {
                listWindow.SetContentColor(Color::OverlayContent);
                listWindow.SetFocusedContentColor(Color::OverlayContent);
                listWindow.SetFrameColor(Color::OverlayListFrame);
                listWindow.SetFocusedFrameColor(Color::OverlayListFrameFocused);
                listWindow.SetFrameVisible(frameVisible);
            }

            /** @brief Returns the stack managing this overlay.
             *  @return the OverlayStack pointer, or nullptr.
             */
            OverlayStack* GetOverlayStack() {
                return this->stack;
            }

            /** @brief Called when the overlay is dismissed; subclasses may override. */
            virtual void OnDismissed() {
                /* for subclass use */
            }

        private:
            OverlayStack* stack;   /**< The stack that owns this overlay. */
    };
}
