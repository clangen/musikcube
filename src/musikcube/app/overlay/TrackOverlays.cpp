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

#include <stdafx.h>

#include "TrackOverlays.h"
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/App.h>
#include <app/util/Rating.h>
#include <map>
#include <set>

using namespace cursespp;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::db;

static const int kDefaultSortOverlayWidth = 32;
static const int kDefaultRatingOverlayWidth = 24;

static const std::map<mc_track_search_order, std::string> sortDisplayKeys = {
    { MC_TRACK_SEARCH_TITLE, "track_list_sort_title" },
    { MC_TRACK_SEARCH_ALBUM, "track_list_sort_album" },
    { MC_TRACK_SEARCH_ARTIST, "track_list_sort_artist" },
    { MC_TRACK_SEARCH_DATE_ADDED_ASC, "track_list_sort_date_added_asc" },
    { MC_TRACK_SEARCH_DATE_ADDED_DESC, "track_list_sort_date_added_desc" },
    { MC_TRACK_SEARCH_DATE_UPDATED_ASC, "track_list_sort_date_updated_asc" },
    { MC_TRACK_SEARCH_DATE_UPDATED_DESC, "track_list_sort_date_updated_desc" },
    { MC_TRACK_SEARCH_LAST_PLAYED_ASC, "track_list_sort_last_played_asc" },
    { MC_TRACK_SEARCH_LAST_PLAYED_DESC, "track_list_sort_last_played_desc" },
    { MC_TRACK_SEARCH_RATING_ASC, "track_list_sort_rating_asc" },
    { MC_TRACK_SEARCH_RATING_DESC, "track_list_sort_rating_desc" },
    { MC_TRACK_SEARCH_PLAY_COUNT_ASC, "track_list_sort_play_count_asc" },
    { MC_TRACK_SEARCH_PLAY_COUNT_DESC, "track_list_sort_play_count_desc" },
    { MC_TRACK_SEARCH_GENRE, "track_list_sort_genre" },
};

std::string TrackOverlays::TrackSortName(mc_track_search_order order) {
    return _TSTR(sortDisplayKeys.at(order));
}

void TrackOverlays::ShowTrackSearchSortOverlay(
    mc_track_search_order sortType,
    std::function<void(mc_track_search_order)> callback)
{
    size_t i = 0;
    size_t selectedIndex = 0;
    std::vector<mc_track_search_order> allKeys;
    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (auto it : sortDisplayKeys) {
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

void TrackOverlays::ShowRateTrackOverlay(int currentRating, std::function<void(uint32_t)> callback) {
    currentRating = std::max(0, std::min(5, currentRating));
    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (int i = 0; i <= 5; i++) {
        adapter->AddEntry(getRatingString(i));
    }

    auto dialog = std::make_shared<ListOverlay>();

    auto selectRating = [callback = std::move(callback)](ListOverlay* overlay, uint32_t rating) {
        const auto selected = callback;
        overlay->Dismiss();
        selected(rating);
    };

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("track_list_rate_track_overlay_title"))
        .SetWidth(_DIMEN("track_list_rate_track_width", kDefaultRatingOverlayWidth))
        .SetSelectedIndex((int) currentRating)
        .SetKeyInterceptorCallback(
            [selectRating](ListOverlay* overlay, std::string key) -> bool {
                if (key.size() == 1 && key.front() >= '0' && key.front() <= '5') {
                    selectRating(overlay, static_cast<uint32_t>(key.front() - '0'));
                    return true;
                }
                return false;
            })
        .SetItemSelectedCallback(
            [selectRating]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                selectRating(overlay, static_cast<uint32_t>(index));
            });

    cursespp::App::Overlays().Push(dialog);
}
