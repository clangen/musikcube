#pragma once

#include "curses_config.h"

#include "ListWindow.h"
#include "TrackListViewQuery.h"
#include "ScrollAdapterBase.h"

#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;

class TrackListView : public ListWindow, public sigslot::has_slots<> {
    public:
        TrackListView(LibraryPtr library, IWindow *parent = NULL);
        ~TrackListView();

        virtual void ProcessMessage(IWindowMessage &message);
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
        LibraryPtr library;
};