#pragma once

#include "stdafx.h"
#include "Colors.h"
#include "SingleLineEntry.h"
#include "TrackListView.h"
#include "IWindowMessage.h"

#include <boost/format.hpp>
#include <boost/format/group.hpp>
#include <iomanip>

#define WINDOW_MESSAGE_QUERY_COMPLETED 1002

using musik::core::IQuery;

TrackListView::TrackListView(LibraryPtr library, IWindow *parent) 
: ListWindow(parent) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->library = library;
    this->library->QueryCompleted.connect(this, &TrackListView::OnQueryCompleted);
    this->adapter = new Adapter(*this);
}

TrackListView::~TrackListView() {

}

void TrackListView::Requery(const std::string& column, DBID id) {
    this->query.reset(new TrackListViewQuery(this->library, column, id));
    this->library->Enqueue(this->query);
}

void TrackListView::OnQueryCompleted(QueryPtr query) {
    if (query == this->query) {
        Post(WINDOW_MESSAGE_QUERY_COMPLETED);
    }
}

void TrackListView::ProcessMessage(IWindowMessage &message) {
    if (message.MessageType() == WINDOW_MESSAGE_QUERY_COMPLETED) {
        if (this->query && this->query->GetStatus() == IQuery::Finished) {
            this->metadata = this->query->GetResult();
            this->query.reset();
            this->OnAdapterChanged();
        }
    }
}

IScrollAdapter& TrackListView::GetScrollAdapter() {
    return *this->adapter;
}

TrackListView::Adapter::Adapter(TrackListView &parent)
: parent(parent) {
}

size_t TrackListView::Adapter::GetEntryCount() {
    return parent.metadata ? parent.metadata->size() : 0;
}

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(size_t index) {
    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;

    TrackPtr track = parent.metadata->at(index);
    std::string trackNum = track->GetValue("track");
    std::string title = track->GetValue("title");

    std::string text = boost::str(
        boost::format("%s  %s") 
            % boost::io::group(std::setw(3), std::setfill(' '), trackNum)
            % title);

    IScrollAdapter::EntryPtr entry(new SingleLineEntry(text));

    entry->SetAttrs(attrs);

    return entry;
}