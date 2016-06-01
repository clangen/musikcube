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
        class TrackListView :
            public cursespp::ListWindow,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<TrackListView>,
#endif
            public sigslot::has_slots<>
        {
            public:
                TrackListView(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library);

                virtual ~TrackListView();

                virtual void ProcessMessage(cursespp::IMessage &message);
                virtual bool KeyPress(const std::string& key);

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
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);
                void OnPlaybackEvent(int eventType);

                std::shared_ptr<TrackListViewQuery> query;
                std::shared_ptr<std::vector<musik::core::TrackPtr> > metadata;
                std::shared_ptr<std::set<size_t> > headers;
                Adapter* adapter;
                PlaybackService& playback;
                musik::core::TrackPtr playing;
                musik::core::LibraryPtr library;
        };
    }
}
