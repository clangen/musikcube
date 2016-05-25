#include "stdafx.h"

#include "SimpleScrollAdapter.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include <boost/algorithm/string.hpp>
#include <utf8/utf8/unchecked.h>

using namespace cursespp;

#define MAX_ENTRY_COUNT 0xffffffff

typedef IScrollAdapter::EntryPtr EntryPtr;

SimpleScrollAdapter::SimpleScrollAdapter() {
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

void SimpleScrollAdapter::AddEntry(std::shared_ptr<IEntry> entry) {
    entry->SetWidth(this->GetWidth());
    entries.push_back(entry);

    while (entries.size() > this->maxEntries) {
        entries.pop_front();
    }
}