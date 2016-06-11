//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

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
    if (this->activeQuery) {
        this->activeQuery->Cancel();
    }

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
        if (this->activeQuery && this->activeQuery->GetStatus() == IQuery::Finished) {
            this->metadata = activeQuery->GetResult();
            activeQuery.reset();
            this->OnAdapterChanged();
            this->OnInvalidated();
        }
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

    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1LL;
    std::shared_ptr<SingleLineEntry> entry(new SingleLineEntry(value));
    entry->SetAttrs(attrs);
    return entry;
}
