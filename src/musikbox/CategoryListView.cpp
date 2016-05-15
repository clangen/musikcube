#pragma once

#include "stdafx.h"
#include "Colors.h"
#include "CategoryListView.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include "CategoryListQuery.h"

using musik::core::LibraryPtr;
using musik::core::IQuery;

CategoryListView::CategoryListView(LibraryPtr library, IWindow *parent)
: ListWindow(parent) {
    this->library = library;
    this->adapter = new Adapter(*this);
    this->activeQuery.reset(new CategoryListQuery());
    this->library->Enqueue(activeQuery);
}

CategoryListView::~CategoryListView() {
    delete adapter;
}

void CategoryListView::OnIdle() {
    if (activeQuery && activeQuery->GetStatus() == IQuery::Finished) {
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
    IScrollAdapter::EntryPtr entry(new SingleLineEntry(parent.metadata->at(index)));
    entry->SetAttrs(attrs);

    return entry;
}