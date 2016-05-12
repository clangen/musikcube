#pragma once

#include "stdafx.h"
#include "Colors.h"
#include "CategoryListView.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include "CategoryListQuery.h"

using musik::core::LibraryPtr;
using musik::core::IQuery;

CategoryListView::CategoryListView(LibraryPtr library) 
: ScrollableWindow() {
    this->library = library;
    this->adapter = new Adapter(*this);
    this->activeQuery = QueryPtr(new CategoryListQuery());
    this->library->Enqueue(activeQuery);
}

CategoryListView::~CategoryListView() {
    delete adapter;
}

void CategoryListView::OnIdle() {
    if (activeQuery && activeQuery->GetStatus() == IQuery::Finished) {
        /* need to make better use of smart pointers here... there should
        be a way to "cast" smart_ptr<Query> to smart_ptr<FooQuery>. */
        CategoryListQuery *clq = (CategoryListQuery *) activeQuery.get();
        this->metadata = clq->GetResult();
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
    this->spos = this->parent.GetScrollPosition();
    int64 attrs = index == this->spos.logicalIndex ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;
    
    IScrollAdapter::EntryPtr entry(new SingleLineEntry(parent.metadata->at(index)));
    entry->SetAttrs(attrs);

    return entry;
}