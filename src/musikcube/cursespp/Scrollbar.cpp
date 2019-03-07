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

#include <stdafx.h>
#include <algorithm>
#include <cursespp/Scrollbar.h>

using namespace cursespp;

void Scrollbar::Draw(ListWindow* list, Window* target) {
    if (!list) {
        return;
    }

    auto *adapter = &list->GetScrollAdapter();
    if (adapter) {
        target = (target == nullptr) ? list : target;

        int trackHeight = target->GetHeight();

        if (target->IsFrameVisible()) {
            trackHeight -= 2; /* account for border */
        }

        if (trackHeight > 1) {
            WINDOW* window = target->GetFrame();
            if (window) {
                auto& pos = list->GetScrollPosition();
                size_t itemCount = adapter->GetEntryCount();

                /* track */
                int trackX = target->GetWidth() - 1;
                int trackMinY = target->IsFrameVisible() ? 1 : 0;
                int trackMaxY = trackMinY + trackHeight;

                /* thumb */
                int thumbY = -1;
                if ((int) itemCount > trackHeight) {
                    float percent = (float) pos.logicalIndex / (float) itemCount;
                    thumbY = (int) ((float) trackHeight * percent) + trackMinY;
                }

                /* validate */
                assert(trackMinY >= 0);
                assert(trackMaxY <= target->GetHeight());
                assert(trackMaxY > trackMinY);
                assert(trackHeight <= target->GetHeight());

                /* draw */
                mvwvline(window, trackMinY, trackX, 0, trackHeight); /* track */
                if (thumbY >= trackMinY && thumbY < trackMaxY) {
                    mvwaddch(window, thumbY, trackX, ' ' | A_REVERSE); /* handle */
                }
            }
        }
    }
}
