#pragma once

#include <core/library/query/QueryBase.h>
#include "TrackListQueryBase.h"
#include <app/service/PlaybackService.h>

namespace musik {
    namespace box {
        class NowPlayingTrackListQuery : public TrackListQueryBase {
            public:
                NowPlayingTrackListQuery(PlaybackService& playback);
                virtual ~NowPlayingTrackListQuery();

                virtual std::string Name() { return "NowPlayingTrackListQuery"; }
                virtual Result GetResult();
                virtual Headers GetHeaders();
                virtual size_t GetQueryHash();

            protected:
                virtual bool OnRun(musik::core::db::Connection &db);

            private:
                PlaybackService& playback;
                Result result;
                Headers headers;
                size_t hash;
        };
    }
}
