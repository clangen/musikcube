#include "stdafx.h"
#include "Colors.h"
#include "Screen.h"
#include "LibraryLayout.h"

LibraryLayout::LibraryLayout(LibraryPtr library) {
    this->SetSize(Screen::GetWidth(), Screen::GetHeight());
    this->SetPosition(0, 0);
    this->Create();

    this->albumList.reset(new CategoryListView(this, library));
    this->trackList.reset(new TrackListView(this));
    this->Layout();
}

LibraryLayout::~LibraryLayout() {

}

IWindow* LibraryLayout::FocusNext() {
    return this->GetFocus();
}

IWindow* LibraryLayout::FocusPrev() {
    return this->GetFocus();
}

IWindow* LibraryLayout::GetFocus() {
    return this->albumList.get();
}

void LibraryLayout::Layout() {
    this->albumList->SetPosition(0, 0);
    this->albumList->SetSize(20, this->GetHeight());
    this->albumList->Create();

    this->trackList->SetPosition(20, 0);
    this->trackList->SetSize(this->GetWidth() - 20, this->GetHeight());
    this->trackList->Create();
}

void LibraryLayout::OnIdle() {
    this->albumList->OnIdle();
}