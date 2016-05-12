#include "stdafx.h"
#include "Screen.h"
#include "LibraryLayout.h"

LibraryLayout::LibraryLayout(LibraryPtr library) {
    this->albumList.reset(new CategoryListView(library));
    this->trackList.reset(new TrackListView());
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
    this->albumList->SetSize(20, Screen::GetHeight());
    this->albumList->Create();

    this->trackList->SetPosition(20, 0);
    this->trackList->SetSize(Screen::GetWidth() - 20, Screen::GetHeight());
    this->trackList->Create();
}

void LibraryLayout::OnIdle() {
    this->albumList->OnIdle();
}