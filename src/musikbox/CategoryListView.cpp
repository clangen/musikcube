#pragma once

#include "stdafx.h"
#include "Colors.h"
#include "CategoryListView.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include "CategoryListViewQuery.h"
#include "IWindowMessage.h"

using musik::core::LibraryPtr;
using musik::core::IQuery;

#define WINDOW_MESSAGE_QUERY_COMPLETED 1002

CategoryListView::CategoryListView(LibraryPtr library, IWindow *parent)
: ListWindow(parent) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->library = library;
    this->library->QueryCompleted.connect(this, &CategoryListView::OnQueryCompleted);
    this->adapter = new Adapter(*this);
}

CategoryListView::~CategoryListView() {
    delete adapter;
}

void CategoryListView::Requery() {
    this->activeQuery.reset(new CategoryListViewQuery());
    this->library->Enqueue(activeQuery);
}

DBID CategoryListView::GetSelectedId() {
    size_t index = this->GetSelectedIndex();
    if (this->metadata && index < this->metadata->size()) {
        return this->metadata->at(index)->id;
    }
    return -1;
}

void CategoryListView::OnQueryCompleted(QueryPtr query) {
    if (query == this->activeQuery) {
        Post(WINDOW_MESSAGE_QUERY_COMPLETED);
    }
}

void CategoryListView::ProcessMessage(IWindowMessage &message) {
    if (message.MessageType() == WINDOW_MESSAGE_QUERY_COMPLETED) {
        this->metadata = activeQuery->GetResult();
        activeQuery.reset();
        this->OnAdapterChanged();
    }
}

IScrollAdapter& CategoryListView::GetScrollAdapter() {
    return *adapter;
}

CategoryListView::Adapter::Adapter(CategoryListView &parent) 
: parent(parent) {
}

size_t CategoryListView::Adapter::GetEntryCount() {
    return parent.metadata ? parent.metadata->size() : 0;
}

IScrollAdapter::EntryPtr CategoryListView::Adapter::GetEntry(size_t index) {
    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;
    IScrollAdapter::EntryPtr entry(new SingleLineEntry(parent.metadata->at(index)->displayValue));
    entry->SetAttrs(attrs);
    return entry;
}