#pragma once

#include "IRandomAccess.h"

#include "../Query/ListBase.h"
#include "../Query/Tracks.h"
#include "../Track.h"

#include <boost/thread/mutex.hpp>
#include <sigslot/sigslot.h>

#include <list>


namespace musik{ namespace core{
    namespace tracklist {

        class Standard : public IRandomAccess, public sigslot::has_slots<>{
            public:
                Standard(void);
                virtual ~Standard(void);

                musik::core::TrackPtr CurrentTrack();
                musik::core::TrackPtr NextTrack();
                musik::core::TrackPtr PreviousTrack();

                musik::core::TrackPtr operator [](int position);
                int Size();
                void SetCurrentPosition(int position);
                int CurrentPosition();


                void ConnectToQuery(musik::core::Query::ListBase &query);
                void SetTrackMetaKeys(std::set<std::string> metaKeys);
                void SetVisibleTracks(int startPosition,int count);

                sigslot::signal1<std::vector<int>> OnVisibleTracksMetadataEvent;

                void SetTrackCache(int maxTracksCached);

            protected:
                void OnTracks(musik::core::TrackVector *tracks,bool clear);
                void OnTracksMeta(musik::core::TrackVector *tracks);

                void LoadVisible();

                void ClearTrackCache();
                int currentPosition;

                int visibleStartPosition;
                int visibleCount;

                int maxTracksCache;

                std::list<musik::core::TrackPtr> tracksCache;

                musik::core::TrackVector tracks;

                boost::mutex mainMutex;

                musik::core::Query::Tracks tracksQuery;

        };
    }
} }

