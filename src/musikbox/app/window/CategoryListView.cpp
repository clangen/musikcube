#include "stdafx.h"
#include <cursespp/Colors.h>

#include <cursespp/SingleLineEntry.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/IMessage.h>

#include <core/library/LocalLibraryConstants.h>

#include <app/util/Text.h>
#include <app/query/CategoryListViewQuery.h>

#include "CategoryListView.h"

using musik::core::LibraryPtr;
using musik::core::IQuery;
using namespace musik::core::library::constants;
using namespace musik::box;
using cursespp::SingleLineEntry;

#define WINDOW_MESSAGE_QUERY_COMPLETED 1002

CategoryListView::CategoryListView(LibraryPtr library, const std::string& fieldName)
: ListWindow(NULL) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->library = library;
    this->library->QueryCompleted.connect(this, &CategoryListView::OnQueryCompleted);
    this->fieldName = fieldName;
    this->adapter = new Adapter(*this);
}

CategoryListView::~CategoryListView() {
    delete adapter;
}

void CategoryListView::Requery() {
    this->activeQuery.reset(new CategoryListViewQuery(this->fieldName));
    this->library->Enqueue(activeQuery);
}

DBID CategoryListView::GetSelectedId() {
    size_t index = this->GetSelectedIndex();
    if (index != NO_SELECTION && this->metadata && index < this->metadata->size()) {
        return this->metadata->at(index)->id;
    }
    return -1;
}

std::string CategoryListView::GetFieldName() {
    return this->fieldName;
}

void CategoryListView::SetFieldName(const std::string& fieldName) {
    if (this->fieldName != fieldName) {
        this->fieldName = fieldName;

        if (this->metadata) {
            this->metadata.reset();
            //this->OnAdapterChanged();
        }

        this->Requery();
    }
}

void CategoryListView::OnQueryCompleted(QueryPtr query) {
    if (query == this->activeQuery) {
        this->PostMessage(WINDOW_MESSAGE_QUERY_COMPLETED);
    }
}

void CategoryListView::ProcessMessage(IMessage &message) {
    if (message.Type() == WINDOW_MESSAGE_QUERY_COMPLETED) {
        this->metadata = activeQuery->GetResult();
        activeQuery.reset();
        this->OnAdapterChanged();
        this->OnInvalidated();
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
    std::string value = parent.metadata->at(index)->displayValue;
    text::Ellipsize(value, this->GetWidth());

    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;
    IScrollAdapter::EntryPtr entry(new SingleLineEntry(value));
    entry->SetAttrs(attrs);
    return entry;
}
