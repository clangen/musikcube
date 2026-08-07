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

/** @file AppLayout.h @brief Root layout that hosts a content layout and the shortcuts bar. */
#pragma once

#include <cursespp/App.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/ITopLevelLayout.h>

#include <sigslot/sigslot.h>

namespace cursespp {
    /** @brief The root layout of a view, combining a content layout and a ShortcutsWindow.
     *
     *  @details AppLayout sits directly beneath the application's root window.
     *  It owns an inner LayoutBase that fills the content area and a
     *  ShortcutsWindow ("command bar") rendered along the bottom edge. Focus
     *  alternates between the content layout and the shortcuts bar; the
     *  shortcuts bar can be hidden automatically while the user types. It
     *  participates in sigslot notifications from the owned layout so it can
     *  update the shortcut bar contents when the inner layout changes.
     */
    class AppLayout:
        public cursespp::LayoutBase,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Creates the root layout bound to the application.
             *  @param app the owning App instance.
             */
            AppLayout(cursespp::App& app);

            /** @brief Destroys the layout and its children. */
            virtual ~AppLayout();

            /** @brief Dispatches a key press to the focused child.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            virtual bool KeyPress(const std::string& key) override;
            /** @brief Arranges the inner layout and the shortcuts bar. */
            virtual void OnLayout() override;
            /** @brief Returns the currently focused window.
             *  @return the focused IWindowPtr, or nullptr.
             */
            virtual cursespp::IWindowPtr GetFocus() override;
            /** @brief Moves focus to the next focusable window.
             *  @return the newly focused window.
             */
            virtual cursespp::IWindowPtr FocusNext() override;
            /** @brief Moves focus to the previous focusable window.
             *  @return the newly focused window.
             */
            virtual cursespp::IWindowPtr FocusPrev() override;

            /** @brief Replaces the inner content layout.
             *  @param layout the new LayoutBase to display.
             */
            void SetLayout(std::shared_ptr<cursespp::LayoutBase> layout);
            /** @brief Returns the inner content layout.
             *  @return the current content LayoutBase.
             */
            std::shared_ptr<cursespp::LayoutBase> GetLayout() { return this->layout; }

            /** @brief Controls whether the command bar auto-hides while typing.
             *  @param autoHide true to hide the bar during input.
             */
            void SetAutoHideCommandBar(bool autoHide);
            /** @brief Returns whether the command bar auto-hides while typing.
             *  @return true if auto-hide is enabled.
             */
            bool GetAutoHideCommandBar();

        protected:
            /** @brief Sets the padding around the content area.
             *  @param t top padding.
             *  @param l left padding.
             *  @param b bottom padding.
             *  @param r right padding.
             */
            virtual void SetPadding(size_t t, size_t l, size_t b, size_t r);

        private:
            void Initialize();

            void EnableDemoModeIfNecessary();

            cursespp::IWindowPtr BlurShortcuts();
            void FocusShortcuts();

            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;  /**< The shortcuts/command bar window. */
            std::shared_ptr<cursespp::LayoutBase> layout;          /**< The inner content layout. */
            std::shared_ptr<cursespp::TextLabel> hotkey;           /**< Label used to render the current hotkey hint. */
            cursespp::IWindowPtr lastFocus;                        /**< Focus preserved while the command bar is active. */
            ITopLevelLayout* topLevelLayout;                       /**< Top-level layout that owns this bar, if any. */
            size_t paddingT{0}, paddingL{0}, paddingB{0}, paddingR{0}; /**< Content padding in cells. */
            bool autoHideCommandBar{ false };                      /**< Whether the command bar hides during input. */
    };
}
