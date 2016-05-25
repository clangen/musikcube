#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/ListWindow.h>

#include <app/query/TrackListViewQuery.h>
#include <app/service/PlaybackService.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;
using musik::core::audio::Transport;

namespace musik {
    namespace box {
        class TrackListView : public ListWindow, public sigslot::has_slots<> {
            public:
                TrackListView(PlaybackService& playback, LibraryPtr library, IWindow *parent = NULL);
                ~TrackListView();

                virtual void ProcessMessage(IMessage &message);
                virtual bool KeyPress(int64 ch);

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
                PlaybackService& playback;
                LibraryPtr library;
        };
    }
}
