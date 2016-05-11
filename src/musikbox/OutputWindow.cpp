#pragma once

#include "stdafx.h"
#include "OutputWindow.h"
#include "Screen.h"
#include "Colors.h"
#include "MultiLineEntry.h"

typedef IScrollAdapter::IEntry IEntry;

OutputWindow::OutputWindow() 
: ScrollableWindow()
{
    this->SetContentColor(BOX_COLOR_BLACK_ON_GREY);

    this->adapter = new SimpleScrollAdapter();
    this->adapter->SetDisplaySize(this->GetContentWidth(), this->GetContentHeight());
    this->adapter->SetMaxEntries(500);
}

OutputWindow::~OutputWindow() {

}

IScrollAdapter& OutputWindow::GetScrollAdapter() {
    return (IScrollAdapter&) *this->adapter;
}

void OutputWindow::WriteLine(const std::string& text, int64 attrs) {
    this->adapter->AddEntry(boost::shared_ptr<IEntry>(new MultiLineEntry(text, attrs)));
    this->OnAdapterChanged();
}