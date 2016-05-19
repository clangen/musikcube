#pragma once

#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/IWindowMessage.h>

#include "TrackListView.h"

#include <core/library/LocalLibraryConstants.h>

#include <app/util/Text.h>

#include <boost/format.hpp>
#include <boost/format/group.hpp>
#include <boost/lexical_cast.hpp>

#include <iomanip>

#define WINDOW_MESSAGE_QUERY_COMPLETED 1002

using musik::core::IQuery;
using musik::core::audio::Transport;
using namespace musik::core::library::constants;
using namespace musik::box;

using boost::io::group;
using std::setw;
using std::setfill;
using std::setiosflags;

TrackListView::TrackListView(Transport& transport, LibraryPtr library, IWindow *parent) 
: ListWindow(parent) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->transport = &transport;
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

bool TrackListView::KeyPress(int64 ch) {
    if (ch == '\n') { /* return */
        size_t selected = this->GetSelectedIndex();
        if (this->metadata && this->metadata->size() > selected) {
            TrackPtr track = this->metadata->at(selected);
            std::string fn = track->GetValue(Track::FILENAME);
            this->transport->Stop();
            this->transport->Start(fn);
            return true;
        }
    }

    return ListWindow::KeyPress(ch);
}

void TrackListView::ProcessMessage(IWindowMessage &message) {
    if (message.MessageType() == WINDOW_MESSAGE_QUERY_COMPLETED) {
        if (this->query && this->query->GetStatus() == IQuery::Finished) {
            this->metadata = this->query->GetResult();
            this->query.reset();
            this->SetSelectedIndex(0);
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

#define TRACK_NUM_LENGTH 3
#define ARTIST_LENGTH 14
#define ALBUM_LENGTH 14
#define DURATION_LENGTH 5 /* 00:00 */

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(size_t index) {
    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;

    TrackPtr track = parent.metadata->at(index);
    std::string trackNum = track->GetValue(Track::TRACK_NUM);
    std::string artist = track->GetValue(Track::ARTIST_ID);
    std::string album = track->GetValue(Track::ALBUM_ID);
    std::string title = track->GetValue(Track::TITLE);
    std::string duration = track->GetValue(Track::DURATION);

    size_t titleCount = 
        this->GetWidth() - 
        TRACK_NUM_LENGTH - 
        ARTIST_LENGTH - 
        ALBUM_LENGTH - 
        DURATION_LENGTH - 
        (3 * 4); /* 3 = spacing */

    text::Ellipsize(artist, ARTIST_LENGTH);
    text::Ellipsize(album, ALBUM_LENGTH);
    text::Ellipsize(title, titleCount);
    text::Duration(duration);

    std::string text = boost::str(
        boost::format("%s   %s   %s   %s   %s") 
            % group(setw(TRACK_NUM_LENGTH), setfill(' '), trackNum)
            % group(setw(titleCount), setiosflags(std::ios::left), setfill(' '), title)
            % group(setw(DURATION_LENGTH), setiosflags(std::ios::right), setfill(' '), duration)
            % group(setw(ARTIST_LENGTH), setiosflags(std::ios::left), setfill(' '), artist)
            % group(setw(ALBUM_LENGTH), setiosflags(std::ios::left), setfill(' '), album));

    IScrollAdapter::EntryPtr entry(new SingleLineEntry(text));

    entry->SetAttrs(attrs);

    return entry;
}