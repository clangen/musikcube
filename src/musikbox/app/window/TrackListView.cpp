#pragma once

#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/IMessage.h>

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
        this->PostMessage(WINDOW_MESSAGE_QUERY_COMPLETED);
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

void TrackListView::ProcessMessage(IMessage &message) {
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

#define TRACK_COL_WIDTH 3
#define ARTIST_COL_WIDTH 14
#define ALBUM_COL_WIDTH 14
#define DURATION_COL_WIDTH 5 /* 00:00 */

/* so this part is a bit tricky... we draw multiple columns, but we use
standard std::setw() stuff, which is not aware of multi-byte characters.
so we have to manually adjust the widths (i.e. we can't just use simple
constants) */
#define DISPLAY_WIDTH(chars, str) \
    chars + (str.size() - u8len(str))

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(size_t index) {
    int64 attrs = (index == parent.GetSelectedIndex()) ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN) : -1;

    TrackPtr track = parent.metadata->at(index);
    std::string trackNum = track->GetValue(Track::TRACK_NUM);
    std::string artist = track->GetValue(Track::ARTIST_ID);
    std::string album = track->GetValue(Track::ALBUM_ID);
    std::string title = track->GetValue(Track::TITLE);
    std::string duration = track->GetValue(Track::DURATION);


    int column0Width = DISPLAY_WIDTH(TRACK_COL_WIDTH, trackNum);
    int column2Width = DISPLAY_WIDTH(DURATION_COL_WIDTH, duration);
    int column3Width = DISPLAY_WIDTH(ARTIST_COL_WIDTH, artist);
    int column4Width = DISPLAY_WIDTH(ALBUM_COL_WIDTH, album);

    size_t column1CharacterCount =
        this->GetWidth() -
        column0Width -
        column2Width -
        column3Width -
        column4Width -
        (3 * 4); /* 3 = spacing */

    int column1Width = DISPLAY_WIDTH(column1CharacterCount, title);

    text::Ellipsize(artist, ARTIST_COL_WIDTH);
    text::Ellipsize(album, ALBUM_COL_WIDTH);
    text::Ellipsize(title, column1CharacterCount);
    duration = text::Duration(duration);

    std::string text = boost::str(
        boost::format("%s   %s   %s   %s   %s") 
            % group(setw(column0Width), setfill(' '), trackNum)
            % group(setw(column1Width), setiosflags(std::ios::left), setfill(' '), title)
            % group(setw(column2Width), setiosflags(std::ios::right), setfill(' '), duration)
            % group(setw(column3Width), setiosflags(std::ios::left), setfill(' '), artist)
            % group(setw(column4Width), setiosflags(std::ios::left), setfill(' '), album));

    IScrollAdapter::EntryPtr entry(new SingleLineEntry(text));

    entry->SetAttrs(attrs);

    return entry;
}