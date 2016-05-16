#include "stdafx.h"
#include "Colors.h"
#include "Screen.h"
#include "LibraryLayout.h"

LibraryLayout::LibraryLayout(LibraryPtr library) 
: LayoutBase() {
    this->library = library;
    this->InitializeWindows();
}

LibraryLayout::~LibraryLayout() {

}

void LibraryLayout::Layout() {
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);

    this->albumList->SetPosition(0, 0);
    this->albumList->SetSize(20, this->GetHeight());
    this->albumList->SetFocusOrder(0);

    this->trackList->SetPosition(20, 0);
    this->trackList->SetSize(this->GetWidth() - 20, this->GetHeight());
    this->trackList->SetFocusOrder(1);
}

void LibraryLayout::InitializeWindows() {
    this->albumList.reset(new CategoryListView(library));
    this->trackList.reset(new TrackListView(library));

    this->AddWindow(this->albumList);
    this->AddWindow(this->trackList);

    this->albumList->SelectionChanged.connect(
        this, &LibraryLayout::OnCategoryViewSelectionChanged);

    this->Layout();
}

void LibraryLayout::Show() {
    LayoutBase::Show();
    this->albumList->Requery();
}

void LibraryLayout::OnCategoryViewSelectionChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex) {
    if (view == this->albumList.get()) {
        DBID id = this->albumList->GetSelectedId();
        if (id != -1) {
            this->trackList->Requery("album_id", id);
        }
    }
}