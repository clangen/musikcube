//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "stdafx.h"

#include "PlayQueueOverlays.h"

#include <core/audio/Visualizer.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

using namespace musik::box;
using namespace musik::core;
using namespace musik::core::audio;
using namespace cursespp;

using Adapter = cursespp::SimpleScrollAdapter;

PlayQueueOverlays::PlayQueueOverlays() {

}

void PlayQueueOverlays::ShowAddTrackOverlay(PlaybackService& playback, TrackListView& trackList) {
    size_t selectedIndex = trackList.GetSelectedIndex();

    if (selectedIndex == (size_t)-1) {
        return;
    }

    DBID trackId = trackList.Get(selectedIndex)->Id();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry("add to end");
    adapter->AddEntry("add as next");
    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle("add to play queue")
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
            [trackId, &playback](cursespp::IScrollAdapterPtr adapter, size_t index) {
                auto editor = playback.Edit();
                if (index == 0) { /* end */
                    editor.Add(trackId);
                }
                else { /* next */
                    size_t position = playback.GetIndex();
                    if (position == (size_t)-1) {
                        editor.Add(trackId);
                    }
                    else {
                        editor.Insert(trackId, position + 1);
                    }
                }
            });

    cursespp::App::Overlays().Push(dialog);
}