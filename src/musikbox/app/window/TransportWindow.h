#pragma once

#include <cursespp/Window.h>
#include <core/playback/Transport.h>
#include <core/library/Track/Track.h>
#include <core/library/ILibrary.h>
#include <app/query/SingleTrackQuery.h>
#include <sigslot/sigslot.h>
#include "OutputWindow.h"

using namespace musik::core::audio;
using musik::core::TrackPtr;
using musik::core::LibraryPtr;
using musik::core::QueryPtr;

class TransportWindow : public Window, public sigslot::has_slots<> {
    public:
        TransportWindow(LibraryPtr library, Transport& transport);
        ~TransportWindow();

        virtual void ProcessMessage(IWindowMessage &message);
        virtual void Show();
        void Update();

    private:
        void OnTransportPlaybackEvent(int eventType, std::string url);
        void OnTransportVolumeChanged();
        void OnQueryCompleted(QueryPtr query);

        bool paused;
        LibraryPtr library;
        Transport* transport;
        TrackPtr currentTrack;
        std::shared_ptr<SingleTrackQuery> trackQuery;
};