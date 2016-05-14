#include "stdafx.h"
#include "Colors.h"
#include "Screen.h"
#include "LibraryLayout.h"

LibraryLayout::LibraryLayout(LibraryPtr library) 
: LayoutBase() {
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->Show();

    this->albumList.reset(new CategoryListView(NULL, library));
    this->albumList->SetFocusOrder(0);

    this->trackList.reset(new TrackListView(NULL));
    this->trackList->SetFocusOrder(1);

    this->AddWindow(this->albumList);
    this->AddWindow(this->trackList);

    this->Layout();
}

LibraryLayout::~LibraryLayout() {

}

void LibraryLayout::Layout() {
    this->albumList->SetPosition(0, 0);
    this->albumList->SetSize(20, this->GetHeight());
    this->albumList->Show();

    this->trackList->SetPosition(20, 0);
    this->trackList->SetSize(this->GetWidth() - 20, this->GetHeight());
    this->trackList->Show();
}

void LibraryLayout::OnIdle() {
    this->albumList->OnIdle();
}