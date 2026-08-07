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

/** @file ILayout.h @brief Interface for container widgets that arrange and focus child windows. */
#pragma once

#include <cursespp/IWindowGroup.h>
#include <cursespp/IDisplayable.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/IMouseHandler.h>
#include <memory>

namespace cursespp {
    /** @brief Contract for layouts -- containers that manage a set of child windows.
     *
     *  @details A layout owns a collection of windows, arranges their geometry
     *  (typically via an app's Layout()), and manages keyboard focus among
     *  them. It acts as both an IWindowGroup and a key/mouse handler so that
     *  input can be routed to the focused child. Focus navigation follows a
     *  linear ordering of "focusable" windows; depending on the FocusMode,
     *  navigating past the last window either wraps around to the first or
     *  terminates at the edge.
     */
    class ILayout:
        public IWindowGroup,
        public IKeyHandler,
        public IMouseHandler,
        public IOrderable,
        public IDisplayable
    {
        public:
            /** @brief Controls what happens when focus navigation reaches an edge. */
            enum FocusMode {
                FocusModeCircular = 0,      /**< Wraps focus around to the opposite edge. */
                FocusModeTerminating = 1    /**< Stops focus at the last/first window. */
            };

            virtual ~ILayout() { }

            /** @brief Moves focus to the first focusable window.
             *  @return the window that received focus.
             */
            virtual IWindowPtr FocusFirst() = 0;

            /** @brief Moves focus to the last focusable window.
             *  @return the window that received focus.
             */
            virtual IWindowPtr FocusLast() = 0;

            /** @brief Moves focus to the next focusable window.
             *  @return the window that received focus.
             */
            virtual IWindowPtr FocusNext() = 0;

            /** @brief Moves focus to the previous focusable window.
             *  @return the window that received focus.
             */
            virtual IWindowPtr FocusPrev() = 0;

            /** @brief Returns the window that currently has focus.
             *  @return the focused window, or nullptr if none is focused.
             */
            virtual IWindowPtr GetFocus() = 0;

            /** @brief Returns the index of the currently focused window.
             *  @return the focus index into the layout's focusable list.
             */
            virtual int GetFocusIndex() = 0;

            /** @brief Sets focus to the given window.
             *  @param window the window to focus.
             *  @return true if the window was found and focused.
             */
            virtual bool SetFocus(IWindowPtr window) = 0;

            /** @brief Sets focus to the window at the given index.
             *  @param index the focus index to select.
             *  @param applyFocus true to actually focus the window, false to only record the index.
             */
            virtual void SetFocusIndex(int index, bool applyFocus = true) = 0;

            /** @brief Returns the number of focusable windows in the layout.
             *  @return the count of focusable children.
             */
            virtual int GetFocusableCount() = 0;

            /** @brief Returns the focusable window at a given index.
             *  @param index the focus index.
             *  @return the window at that index.
             */
            virtual IWindowPtr GetFocusableAt(int index) = 0;

            /** @brief Returns the current focus navigation mode.
             *  @return the active FocusMode.
             */
            virtual FocusMode GetFocusMode() const = 0;

            /** @brief Sets the focus navigation mode.
             *  @param mode the FocusMode to use.
             */
            virtual void SetFocusMode(FocusMode mode) = 0;

            /** @brief Arranges all child windows based on the layout's geometry. */
            virtual void Layout() = 0;
    };

    /** @brief Shared-pointer alias for ILayout. */
    typedef std::shared_ptr<ILayout> ILayoutPtr;
}
