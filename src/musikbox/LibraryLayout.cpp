#include "stdafx.h"
#include "Colors.h"
#include "Screen.h"
#include "LibraryLayout.h"

LibraryLayout::LibraryLayout(LibraryPtr library) 
: LayoutBase() {
    this->library = library;
    this->InitializeViews();
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

void LibraryLayout::OnIdle() {
    this->albumList->OnIdle();
}

void LibraryLayout::InitializeViews() {
    this->albumList.reset(new CategoryListView(library));
    this->trackList.reset(new TrackListView());

    this->AddWindow(this->albumList);
    this->AddWindow(this->trackList);

    this->Layout();
}