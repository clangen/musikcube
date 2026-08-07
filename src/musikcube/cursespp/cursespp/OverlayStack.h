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

/** @file OverlayStack.h @brief A LIFO stack of modal overlay layouts. */
#pragma once

#include <cursespp/ILayout.h>
#include <vector>

namespace cursespp {
    /** @brief Manages the stack of modal overlays shown over the main layout.
     *
     *  @details OverlayStack holds overlays in LIFO order. The top overlay is
     *  the one currently visible and consumes all input while displayed. The
     *  application owns a single stack (accessible via App::Overlays());
     *  overlays are pushed when they are shown and removed via Remove() when
     *  dismissed. Clearing the stack is restricted to friend classes (App).
     */
    class OverlayStack {
        public:
            /** @brief Creates an empty stack. */
            OverlayStack();

            /** @brief Returns the overlay currently on top of the stack.
             *  @return the top ILayoutPtr, or nullptr if the stack is empty.
             */
            ILayoutPtr Top();
            /** @brief Pushes an overlay onto the stack, making it visible.
             *  @param layout the overlay layout to push.
             */
            void Push(ILayoutPtr layout);
            /** @brief Removes a specific overlay by shared pointer.
             *  @param layout the overlay to remove.
             */
            void Remove(ILayoutPtr layout);
            /** @brief Removes a specific overlay by raw pointer.
             *  @param layout the overlay to remove.
             */
            void Remove(ILayout* layout);

        protected:
            friend class App;
            void Clear(); /* don't want this exposed to the public */

        private:
            std::vector<ILayoutPtr> stack;   /**< The ordered overlay list. */
    };
}