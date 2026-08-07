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

/** @file ITopLevelLayout.h @brief Interface for layouts that host an application's root windows. */
#pragma once

#include <cursespp/ShortcutsWindow.h>

namespace cursespp {
    /** @brief Contract for top-level layouts that can own a ShortcutsWindow.
     *
     *  @details A top-level layout fills the entire application area and hosts
     *  the primary content of a view. It optionally owns a ShortcutsWindow,
     *  which renders the contextual key-binding legend at the bottom of the
     *  screen. The layout is responsible for positioning the shortcuts bar and
     *  for redrawing it whenever the shortcut set changes.
     */
    class ITopLevelLayout {
        public:
            virtual ~ITopLevelLayout() { }

            /** @brief Associates a ShortcutsWindow with this layout.
             *  @param w the shortcuts window to display, or nullptr to detach it.
             */
            virtual void SetShortcutsWindow(cursespp::ShortcutsWindow* w) = 0;
    };
}
