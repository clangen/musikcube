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

#include "TrackOverlays.h"
#include <core/library/query/local/util/TrackSort.h>
#include <core/library/query/local/SetTrackRatingQuery.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/App.h>
#include <app/util/Rating.h>
#include <set>

using namespace cursespp;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;

static const int kDefaultSortOverlayWidth = 32;
static const int kDefaultRatingOverlayWidth = 24;

void TrackOverlays::ShowTrackSearchSortOverlay(
    TrackSortType sortType,
    const std::map<TrackSortType, std::string>& availableSortTypes,
    std::function<void(TrackSortType)> callback)
{
    size_t i = 0;
    size_t selectedIndex = 0;
    std::vector<TrackSortType> allKeys;
    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (auto it : availableSortTypes) {
        allKeys.push_back(it.first);
        adapter->AddEntry(_TSTR(it.second));
        if (it.first == sortType) {
            selectedIndex = i;
        }
        ++i;
    }

    auto dialog = std::make_shared<ListOverlay>();
    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("track_list_sort_overlay_title"))
        .SetWidth(_DIMEN("track_search_sort_order_width", kDefaultSortOverlayWidth))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback, allKeys](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                callback(allKeys[index]);
            });

    cursespp::App::Overlays().Push(dialog);
}

void TrackOverlays::ShowRateTrackOverlay(
    musik::core::TrackPtr track,
    musik::core::ILibraryPtr library,
    std::function<void(bool)> callback)
{
    int64_t trackId = track->GetId();
    int currentRating = track->GetInt32("rating", 0);

    currentRating = std::max(0, std::min(5, currentRating));
    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (int i = 0; i <= 5; i++) {
        adapter->AddEntry(getRatingString(i));
    }

    auto dialog = std::make_shared<ListOverlay>();

    auto updateRatingInLibrary = [track, library, callback, dialog](int index) {
        auto query = std::make_shared<SetTrackRatingQuery>(track->GetId(), (int) index);
        library->Enqueue(query, ILibrary::QuerySynchronous);
        dialog->Dismiss();
        callback(true);
    };

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("track_list_rate_track_overlay_title"))
        .SetWidth(_DIMEN("track_list_rate_track_width", kDefaultRatingOverlayWidth))
        .SetSelectedIndex((int) currentRating)
        .SetKeyInterceptorCallback(
            [updateRatingInLibrary](ListOverlay* overlay, std::string key) -> bool {
                if (key == "0") { updateRatingInLibrary(0); return true; }
                if (key == "1") { updateRatingInLibrary(1); return true; }
                if (key == "2") { updateRatingInLibrary(2); return true; }
                if (key == "3") { updateRatingInLibrary(3); return true; }
                if (key == "4") { updateRatingInLibrary(4); return true; }
                if (key == "5") { updateRatingInLibrary(5); return true; }
                return false;
            })
        .SetItemSelectedCallback(
            [updateRatingInLibrary]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                updateRatingInLibrary((int) index);
            });

    cursespp::App::Overlays().Push(dialog);
}