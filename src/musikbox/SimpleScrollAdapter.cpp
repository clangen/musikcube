#include "stdafx.h"

#include "SimpleScrollAdapter.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include <boost/algorithm/string.hpp>
#include <utf8/utf8/unchecked.h>

#define MAX_ENTRY_COUNT 0xffffffff

typedef IScrollAdapter::EntryPtr EntryPtr;

SimpleScrollAdapter::SimpleScrollAdapter() {
    /* the adapters can have a maximum size. as we remove elements from
    the back, we don't want to re-index everything. instead, we'll use
    this offset for future calculations when searching for items. */
    this->removedOffset = 0; 
    this->maxEntries = MAX_ENTRY_COUNT;
}

SimpleScrollAdapter::~SimpleScrollAdapter() {

}

size_t SimpleScrollAdapter::GetEntryCount() {
    return this->entries.size();
}

void SimpleScrollAdapter::SetMaxEntries(size_t maxEntries) {
    this->maxEntries = maxEntries;
}

EntryPtr SimpleScrollAdapter::GetEntry(size_t index) {
    return this->entries.at(index);
}

void SimpleScrollAdapter::AddEntry(boost::shared_ptr<IEntry> entry) {
    entry->SetWidth(this->GetWidth());
    entries.push_back(entry);

    while (entries.size() > this->maxEntries) {
        entries.pop_front();
    }
}