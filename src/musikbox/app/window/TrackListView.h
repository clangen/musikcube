#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/ListWindow.h>

#include <app/query/TrackListViewQuery.h>
#include <app/service/PlaybackService.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

namespace musik {
    namespace box {
        class TrackListView : public cursespp::ListWindow, public sigslot::has_slots<> {
            public:
                TrackListView(
                    PlaybackService& playback, 
                    musik::core::LibraryPtr library);

                ~TrackListView();

                virtual void ProcessMessage(cursespp::IMessage &message);
                virtual bool KeyPress(int64 ch);

                void Requery(const std::string& column, DBID id);

            protected:
                virtual cursespp::IScrollAdapter& GetScrollAdapter();
                void OnQueryCompleted(musik::core::QueryPtr query);

                class Adapter : public cursespp::ScrollAdapterBase {
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
                musik::core::LibraryPtr library;
        };
    }
}
