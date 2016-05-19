#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <core/library/LocalLibraryConstants.h>

#include "LibraryLayout.h"

using namespace musik::core::library::constants;

#define CATEGORY_WIDTH 25
#define TRANSPORT_HEIGHT 3
#define DEFAULT_CATEGORY Track::ALBUM_ID

LibraryLayout::LibraryLayout(Transport& transport, LibraryPtr library)
: LayoutBase()
, transport(transport) {
    this->library = library;
    this->InitializeWindows();
}

LibraryLayout::~LibraryLayout() {

}

void LibraryLayout::Layout() {
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);

    this->categoryList->SetPosition(0, 0);
    this->categoryList->SetSize(CATEGORY_WIDTH, this->GetHeight() - TRANSPORT_HEIGHT);
    this->categoryList->SetFocusOrder(0);

    this->trackList->SetPosition(CATEGORY_WIDTH, 0);
    this->trackList->SetSize(this->GetWidth() - CATEGORY_WIDTH, this->GetHeight() - TRANSPORT_HEIGHT);
    this->trackList->SetFocusOrder(1);

    this->transportView->SetPosition(1, this->GetHeight() - TRANSPORT_HEIGHT);
    this->transportView->SetSize(this->GetWidth() - 2, TRANSPORT_HEIGHT);
    this->transportView->Update();
}

void LibraryLayout::InitializeWindows() {
    this->categoryList.reset(new CategoryListView(this->library, DEFAULT_CATEGORY));
    this->trackList.reset(new TrackListView(this->transport, this->library));
    this->transportView.reset(new TransportWindow(this->library, this->transport));

    this->AddWindow(this->categoryList);
    this->AddWindow(this->trackList);
    this->AddWindow(this->transportView);

    this->categoryList->SelectionChanged.connect(
        this, &LibraryLayout::OnCategoryViewSelectionChanged);

    this->Layout();
}

void LibraryLayout::Show() {
    LayoutBase::Show();
    this->categoryList->Requery();

    //if (GetOverlay()) {
    //    this->CloseOverlay();
    //}

    //std::shared_ptr<IWindow> overlay(new OutputWindow(NULL));
    //overlay->SetPosition(2, 2);
    //overlay->SetSize(20, 20);
    //this->ShowOverlay(overlay);
}

void LibraryLayout::OnCategoryViewSelectionChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex) {
    if (view == this->categoryList.get()) {
        DBID id = this->categoryList->GetSelectedId();
        if (id != -1) {
            this->trackList->Requery(this->categoryList->GetFieldName(), id);
        }
    }
}

bool LibraryLayout::KeyPress(int64 ch) {
    std::string kn = keyname((int)ch);

    if (kn == "ALT_1") {
        this->categoryList->SetFieldName(Track::ARTIST_ID);
        return true;
    }
    else if (kn == "ALT_2") {
        this->categoryList->SetFieldName(Track::ALBUM_ID);
        return true;
    }
    else if (kn == "ALT_3") {
        this->categoryList->SetFieldName(Track::GENRE_ID);
        return true;
    }

    return LayoutBase::KeyPress(ch);
}