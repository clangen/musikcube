#pragma once

#include "stdafx.h"
#include "OutputWindow.h"
#include "Screen.h"
#include "Colors.h"

OutputWindow::OutputWindow() 
: ScrollableWindow()
{
    this->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 4);
    this->SetPosition(0, 4);
    this->SetContentColor(BOX_COLOR_BLACK_ON_GREY);

    this->adapter = new SimpleScrollAdapter();
    this->adapter->SetDisplaySize(this->GetContentWidth(), this->GetContentHeight());

    this->Create();
}

OutputWindow::~OutputWindow() {

}

IScrollAdapter& OutputWindow::GetScrollAdapter() {
    return (IScrollAdapter&) *this->adapter;
}

void OutputWindow::WriteLine(const std::string& text) {
    this->adapter->AddLine(text);
    this->OnAdapterChanged();
}