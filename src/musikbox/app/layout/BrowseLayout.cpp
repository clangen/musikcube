#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <core/library/LocalLibraryConstants.h>
#include <app/query/CategoryTrackListQuery.h>
#include "BrowseLayout.h"

using namespace musik::core::library::constants;

#define CATEGORY_WIDTH 25
#define DEFAULT_CATEGORY constants::Track::ARTIST_ID

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

BrowseLayout::BrowseLayout(
    PlaybackService& playback, 
    LibraryPtr library)
: LayoutBase()
, playback(playback)
, parent(parent) {
    this->library = library;
    this->InitializeWindows();
}

BrowseLayout::~BrowseLayout() {

}

void BrowseLayout::Layout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();

    if (cx == 0 || cy == 0) {
        return;
    }

    size_t x = this->GetX(), y = this->GetY();

    this->MoveAndResize(x, y, cx, cy);

    this->SetSize(cx, cy);
    this->SetPosition(x, y);

    this->categoryList->MoveAndResize(x, y, CATEGORY_WIDTH, cy);

    this->trackList->MoveAndResize(
        x + CATEGORY_WIDTH, y, cx - CATEGORY_WIDTH, cy);

    this->categoryList->SetFocusOrder(0);
    this->trackList->SetFocusOrder(1);
}

void BrowseLayout::InitializeWindows() {
    this->categoryList.reset(new CategoryListView(this->library, DEFAULT_CATEGORY));
    this->trackList.reset(new TrackListView(this->playback, this->library));

    this->AddWindow(this->categoryList);
    this->AddWindow(this->trackList);

    this->categoryList->SelectionChanged.connect(
        this, &BrowseLayout::OnCategoryViewSelectionChanged);

    this->categoryList->Invalidated.connect(
        this, &BrowseLayout::OnCategoryViewInvalidated);

    this->Layout();
}

IWindowPtr BrowseLayout::GetFocus() {
    return this->focused ? this->focused : LayoutBase::GetFocus();
}

void BrowseLayout::Show() {
    LayoutBase::Show();
    this->categoryList->Requery();
}

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        DBID selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
                new CategoryTrackListQuery(
                    this->library,
                    this->categoryList->GetFieldName(),
                    selectedId)));
        }
    }
}

void BrowseLayout::OnCategoryViewSelectionChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex) {
    this->RequeryTrackList(view);
}

void BrowseLayout::OnCategoryViewInvalidated(
    ListWindow *view, size_t selectedIndex) {
    this->RequeryTrackList(view);
}

bool BrowseLayout::KeyPress(const std::string& key) {
    if (key == "^M") { /* enter. play the selection */
        auto tracks = this->trackList->GetTrackList();
        auto focus = this->GetFocus();

        size_t index = (focus == this->trackList)
            ? this->trackList->GetSelectedIndex() : 0;

        this->playback.Play(*tracks, index);
        return true;
    }
    if (key == "KEY_F(5)") {
        this->categoryList->Requery();
        return true;
    }
    else if (key == "ALT_1" || key == "M-1") {
        this->categoryList->SetFieldName(constants::Track::ARTIST_ID);
        return true;
    }
    else if (key == "ALT_2" || key == "M-2") {
        this->categoryList->SetFieldName(constants::Track::ALBUM_ID);
        return true;
    }
    else if (key == "ALT_3" || key == "M-3") {
        this->categoryList->SetFieldName(constants::Track::GENRE_ID);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
