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

#include "MainController.h"
#include <core/library/LocalLibraryConstants.h>
#include <glue/query/SearchTrackListQuery.h>
#include <glue/util/Playback.h>
#include <glue/util/Duration.h>

using namespace musik::win;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::glue;
using namespace musik::core::library;
using namespace win32cpp;

using ColumnRef = ListView::ColumnRef;

static ColumnRef sNumberColumn = ListView::Column::Create(_T("#"), 50);
static ColumnRef sTitleColumn = ListView::Column::Create(_T("title"));
static ColumnRef sDurationColumn = ListView::Column::Create(_T("duration"), 100, TextAlignment::TextAlignRight);
static ColumnRef sArtistColumn = ListView::Column::Create(_T("artist"));

class TrackListModel : public ListView::Model {
    public:
        static std::shared_ptr<TrackListModel> Create(PlaybackService& playback) {
            return std::shared_ptr<TrackListModel>(new TrackListModel(playback));
        }

        TrackListModel(PlaybackService& playback)
        : playback(playback) {
            this->SetRowCount(playback.Count());
        }

        virtual uistring CellValueToString(int rowIndex, ColumnRef column) {
            TrackPtr track = playback.GetTrackAtIndex(rowIndex);
            if (track) {
                if (column == sNumberColumn) {
                    return u8to16(track->GetValue(constants::Track::TRACK_NUM));
                }
                else if (column == sTitleColumn) {
                    return u8to16(track->GetValue(constants::Track::TITLE));
                }
                else if (column == sDurationColumn) {
                    return u8to16(duration::Duration(track->GetValue(constants::Track::DURATION)));
                }
                else if (column == sArtistColumn) {
                    return u8to16(track->GetValue(constants::Track::ARTIST));
                }
            }
            return _T("");
        }

    private:
        PlaybackService& playback;
};

MainController::MainController(
    MainWindow& mainWindow,
    PlaybackService& playback,
    LibraryPtr library)
: mainWindow(mainWindow)
, playback(playback)
, library(library)  {
    std::shared_ptr<SearchTrackListQuery> query =
        std::shared_ptr<SearchTrackListQuery>(new SearchTrackListQuery(library, "o'o"));

    library->Enqueue(query, ILibrary::QuerySynchronous);

    playback.Play(*query->GetResult(), 0);

    this->categoryList = this->mainWindow.AddChild(new ListView());

    this->trackList = this->mainWindow.AddChild(new ListView());
    this->trackList->AddColumn(sNumberColumn);
    this->trackList->AddColumn(sTitleColumn);
    this->trackList->AddColumn(sDurationColumn);
    this->trackList->AddColumn(sArtistColumn);
    this->trackList->SetModel(TrackListModel::Create(this->playback));

    this->Layout();

    this->mainWindow.Resized.connect(this, &MainController::OnMainWindowResized);
}

MainController::~MainController() {
}

void MainController::Layout() {
    auto size = this->mainWindow.ClientSize();
    if (size.height > 0) {
        static int padding = 8;
        int categoryCx = size.width / 3;
        int tracksCx = size.width - (categoryCx + padding);
        int tracksX = categoryCx + padding;

        this->categoryList->MoveTo(0, 0);
        this->categoryList->Resize(categoryCx, size.height);
        this->trackList->MoveTo(tracksX, 0);
        this->trackList->Resize(tracksCx, size.height);
    }
}

void MainController::OnMainWindowResized(Window* window, Size size) {
    this->Layout();
}