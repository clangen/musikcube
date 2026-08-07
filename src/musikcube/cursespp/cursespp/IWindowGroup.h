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

/** @file IWindowGroup.h @brief Interface for objects that own an ordered collection of windows. */
#pragma once

#include <cursespp/IWindow.h>

namespace cursespp {
    /** @brief Contract for containers that hold a list of child windows.
     *
     *  @details IWindowGroup abstracts the storage of child windows inside a
     *  parent. The children are kept in a linear order (the "z-order"), where
     *  lower positions are drawn first and higher positions are on top. It is
     *  implemented by layouts and other compound widgets that need to manage,
     *  enumerate and reparent child windows.
     */
    class IWindowGroup {
        public:
            virtual ~IWindowGroup() { }

            /** @brief Adds a window to the end of the group.
             *  @param window the window to add.
             *  @return true if the window was added successfully.
             */
            virtual bool AddWindow(IWindowPtr window) = 0;

            /** @brief Removes a window from the group.
             *  @param window the window to remove.
             *  @return true if the window was found and removed.
             */
            virtual bool RemoveWindow(IWindowPtr window) = 0;

            /** @brief Returns the number of windows in the group.
             *  @return the child window count.
             */
            virtual size_t GetWindowCount() = 0;

            /** @brief Returns the window at the given position.
             *  @param position the zero-based index into the child list.
             *  @return the window at that position.
             */
            virtual IWindowPtr GetWindowAt(size_t position) = 0;
    };
}
