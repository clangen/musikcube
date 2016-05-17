#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>

#include <cursespp/ListWindow.h>

#include <app/query/TrackListViewQuery.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;
using musik::core::audio::Transport;

class TrackListView : public ListWindow, public sigslot::has_slots<> {
    public:
        TrackListView(Transport& transport, LibraryPtr library, IWindow *parent = NULL);
        ~TrackListView();

        virtual void ProcessMessage(IWindowMessage &message);
        virtual void KeyPress(int64 ch);

        void Requery(const std::string& column, DBID id);

    protected:
        virtual IScrollAdapter& GetScrollAdapter();
        void OnQueryCompleted(QueryPtr query);

        class Adapter : public ScrollAdapterBase {
        public:
            Adapter(TrackListView &parent);

            virtual size_t GetEntryCount();
            virtual EntryPtr GetEntry(size_t index);

        private:
            TrackListView &parent;
            IScrollAdapter::ScrollPosition spos;
        };

    private:
        std::shared_ptr<TrackListViewQuery> query;
        std::shared_ptr<std::vector<TrackPtr>> metadata;
        Adapter* adapter;
        Transport* transport;
        LibraryPtr library;
};