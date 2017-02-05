//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <app/view/MainWindow.h>

#include <core/runtime/IMessageQueue.h>
#include <core/audio/PlaybackService.h>
#include <core/library/ILibrary.h>

#include <core/library/query/local/TrackListQueryBase.h>

#include <win32cpp/EditView.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/ListView.hpp>

namespace musik { namespace win {
    class MainController :
        public sigslot::has_slots<>,
        public musik::core::runtime::IMessageTarget
    {
        public:
            MainController(
                MainWindow& mainWindow,
                musik::core::audio::PlaybackService& playback,
                musik::core::ILibraryPtr library);

            virtual ~MainController();

            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

        private:
            class TrackListModel;

            void OnMainWindowResized(win32cpp::Window* window, win32cpp::Size size);
            void OnTrackListRowActivated(win32cpp::ListView* list, int index);
            void OnLibraryQueryCompleted(musik::core::db::IQuery* query);
            void OnSearchEditChanged(win32cpp::EditView* editView);
            void OnTransportButtonClicked(win32cpp::Button* button);
            void OnPlaybackStateChanged(int state);

            void Layout();

            musik::core::audio::PlaybackService& playback;
            musik::core::ILibraryPtr library;

            MainWindow& mainWindow;

            std::shared_ptr<musik::core::db::local::TrackListQueryBase> trackListQuery;
            std::shared_ptr<TrackListModel> trackListModel;
            std::shared_ptr<musik::core::TrackList> trackList;

            win32cpp::ListView* trackListView;
            win32cpp::EditView* editView;
            win32cpp::Button* prevButton;
            win32cpp::Button* nextButton;
            win32cpp::Button* pauseButton;

            bool trackListDirty;
    };
} }