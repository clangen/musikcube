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
//#include <core/library/query/local/AllCategoriesQuery.h>
#include <core/library/query/local/util/TrackSort.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/App.h>
#include <set>

using namespace cursespp;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;

static const int kDefaultOverlayWidth = 32;

void TrackOverlays::ShowTrackSearchSortOverlay(
    TrackSortType sortType, std::function<void(TrackSortType)> callback)
{
    size_t i = 0;
    size_t selectedIndex = 0;
    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (auto it : kTrackSortTypeToDisplayKey) {
        adapter->AddEntry(_TSTR(it.second));
        if (it.first == sortType) {
            selectedIndex = i;
        }
        ++i;
    }

    auto dialog = std::make_shared<ListOverlay>();
    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("track_list_sort_overlay_title"))
        .SetWidth(_DIMEN("track_search_sort_order_width", kDefaultOverlayWidth))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                callback((TrackSortType) index);
            });

    cursespp::App::Overlays().Push(dialog);
}

void TrackOverlays::ShowRateTrackOverlay(
    ILibraryPtr library, std::function<void(bool)> callback) {

}