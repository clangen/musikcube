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
#include <core/runtime/Message.h>
#include <core/library/query/local/SearchTrackListQuery.h>
#include <core/support/Playback.h>
#include <core/support/Duration.h>

using namespace musik::win;
using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::audio;
using namespace musik::core::runtime;
using namespace musik::core::db::local;
using namespace musik::core::library;
using namespace win32cpp;

using ColumnRef = ListView::ColumnRef;
using CellRendererRef = ListView::CellRendererRef;
using TextCellRenderer = ListView::TextCellRenderer<uistring>;
using TextCellRendererRef = std::shared_ptr<TextCellRenderer>;

static ColumnRef sNumberColumn = ListView::Column::Create(_T("#"), 50);
static ColumnRef sTitleColumn = ListView::Column::Create(_T("title"), 250);
static ColumnRef sDurationColumn = ListView::Column::Create(_T("time"), 50, TextAlignment::TextAlignRight);
static ColumnRef sAlbumColumn = ListView::Column::Create(_T("album"));
static ColumnRef sArtistColumn = ListView::Column::Create(_T("artist"));
static ColumnRef sEndColumn = ListView::Column::Create(_T(""), 35);

#define EDIT_VIEW_HEIGHT 22
#define UPDATE_SEARCH_MESSAGE 1001
#define UPDATE_SEARCH_DEBOUNCE_MS 750

class MainController::TrackListModel : public ListView::Model, public sigslot::has_slots<> {
    public:
        static std::shared_ptr<TrackListModel> Create(PlaybackService& playback) {
            return std::shared_ptr<TrackListModel>(new TrackListModel(playback));
        }

        TrackListModel(PlaybackService& playback)
        : playback(playback) {
            playback.TrackChanged.connect(this, &TrackListModel::OnTrackChanged);

            this->normal.reset(new TextCellRenderer());

            this->bold.reset(new TextCellRenderer());
            FontRef font = Font::Create();
            font->SetBold(true);
            this->bold->SetFont(font);
        }

        void SetTrackList(std::shared_ptr<TrackList> trackList) {
            this->trackList = trackList;
            this->SetRowCount(trackList ? trackList->Count() : 0);
            this->InvalidateData();
        }

        virtual uistring CellValueToString(int rowIndex, ColumnRef column) {
            return _T("");
        }

        virtual CellRendererRef CellRenderer(int rowIndex, ColumnRef column) {
            TrackPtr track = this->trackList->Get(rowIndex);

            if (track) {
                TextCellRendererRef renderer = this->normal;

                if (playing && playing->GetId() == track->GetId()) {
                    renderer = this->bold;
                }

                uistring value;

                if (column == sNumberColumn) {
                    value = u8to16(track->GetString(constants::Track::TRACK_NUM));
                }
                else if (column == sTitleColumn) {
                    value = u8to16(track->GetString(constants::Track::TITLE));
                }
                else if (column == sDurationColumn) {
                    value = u8to16(duration::Duration(track->GetString(constants::Track::DURATION)));
                }
                else if (column == sAlbumColumn) {
                    value = u8to16(track->GetString(constants::Track::ALBUM));
                }
                else if (column == sArtistColumn) {
                    value = u8to16(track->GetString(constants::Track::ARTIST));
                }

                if (value.size()) {
                    renderer->Set(value, column->Alignment());
                    return renderer;
                }
            }

            return TextCellRendererRef();
        }

    private:
        void OnTrackChanged(size_t index, TrackPtr track) {
            this->playing = track;
            this->InvalidateData();
        }

        std::shared_ptr<TrackList> trackList;
        PlaybackService& playback;
        TextCellRendererRef normal;
        TextCellRendererRef bold;
        TrackPtr playing;
};

MainController::MainController(
    MainWindow& mainWindow,
    PlaybackService& playback,
    ILibraryPtr library)
: mainWindow(mainWindow)
, playback(playback)
, library(library)
, trackListDirty(true) {

    playback.PlaybackEvent.connect(this, &MainController::OnPlaybackStateChanged);
    library->QueryCompleted.connect(this, &MainController::OnLibraryQueryCompleted);

    this->trackListQuery.reset(new SearchTrackListQuery(library, ""));
    library->Enqueue(this->trackListQuery);

    this->editView = this->mainWindow.AddChild(new EditView());
    this->editView->Changed.connect(this, &MainController::OnSearchEditChanged);

    this->trackListView = this->mainWindow.AddChild(new ListView());
    this->trackListView->EnableStripedBackground(true);
    this->trackListView->RowActivated.connect(this, &MainController::OnTrackListRowActivated);
    this->trackListView->AddColumn(sNumberColumn);
    this->trackListView->AddColumn(sTitleColumn);
    this->trackListView->AddColumn(sAlbumColumn);
    this->trackListView->AddColumn(sArtistColumn);
    this->trackListView->AddColumn(sDurationColumn);
    this->trackListView->AddColumn(sEndColumn);

    this->prevButton = this->mainWindow.AddChild(new Button(_T("prev")));
    this->prevButton->Pressed.connect(this, &MainController::OnTransportButtonClicked);

    this->pauseButton = this->mainWindow.AddChild(new Button(_T("play")));
    this->pauseButton->Pressed.connect(this, &MainController::OnTransportButtonClicked);

    this->nextButton = this->mainWindow.AddChild(new Button(_T("next")));
    this->nextButton->Pressed.connect(this, &MainController::OnTransportButtonClicked);

    this->trackListModel = TrackListModel::Create(this->playback);
    this->trackListView->SetModel(this->trackListModel);

    this->Layout();

    this->mainWindow.Resized.connect(this, &MainController::OnMainWindowResized);
}

MainController::~MainController() {
}

#define /*button width */ BW(x) x->WindowSize().width;

void MainController::Layout() {
    auto size = this->mainWindow.ClientSize();
    if (size.height > 0) {
        static int padding = 8;

        int editCx = (size.width * 2) / 3;
        int editX = (size.width / 2) - (editCx / 2);

        this->editView->MoveTo(editX, padding);
        this->editView->Resize(editCx, EDIT_VIEW_HEIGHT);

        int buttonCy = this->prevButton->WindowSize().height;
        int buttonY = size.height - buttonCy - padding;
        int transportCy = padding + buttonCy + padding;
        int buttonX = padding;

        this->prevButton->MoveTo(buttonX, buttonY);
        buttonX += padding + BW(this->prevButton);
        this->pauseButton->MoveTo(buttonX, buttonY);
        buttonX += padding + BW(this->pauseButton);
        this->nextButton->MoveTo(buttonX, buttonY);
        buttonX += padding + BW(this->nextButton);

        int listY = padding + EDIT_VIEW_HEIGHT + padding;
        int tracksCx = size.width;
        int listCy = size.height - listY - transportCy;

        this->trackListView->MoveTo(0, listY);
        this->trackListView->Resize(tracksCx, listCy);
    }
}

void MainController::OnMainWindowResized(Window* window, Size size) {
    this->Layout();
}

void MainController::OnTrackListRowActivated(ListView* list, int index) {
    if (trackListDirty) {
        this->trackListDirty = false;
        if (this->trackList) {
            this->playback.Play((*this->trackList), index);
        }
    }
    else {
        playback.Play(index);
    }
}

void MainController::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == UPDATE_SEARCH_MESSAGE) {
        std::string term = u16to8(this->editView->Caption());
        this->trackListQuery.reset(new SearchTrackListQuery(library, term));
        library->Enqueue(this->trackListQuery);
    }
}

void MainController::OnLibraryQueryCompleted(IQuery* query) {
    if (this->trackListQuery.get() == query) {
        this->trackListDirty = true;
        this->trackList = ((TrackListQueryBase *) query)->GetResult();
        this->trackListModel->SetTrackList(this->trackList);
    }
}

void MainController::OnSearchEditChanged(win32cpp::EditView* editView) {
    if (editView == this->editView) {
        IMessageQueue& queue = mainWindow.Queue();
        queue.Debounce(Message::Create(this, UPDATE_SEARCH_MESSAGE, 0, 0), UPDATE_SEARCH_DEBOUNCE_MS);
    }
}

void MainController::OnTransportButtonClicked(win32cpp::Button* button) {
    if (button == this->prevButton) {
        playback.Previous();
    }
    else if (button == this->pauseButton) {
        if (playback.GetPlaybackState() == musik::core::sdk::PlaybackStopped) {
            std::vector<int> selected = this->trackListView->SelectedRows();
            if (selected.size()) {
                this->OnTrackListRowActivated(this->trackListView, selected[0]);
            }
        }
        else {
            playback.PauseOrResume();
        }
    }
    else if (button == this->nextButton) {
        playback.Next();
    }
}

void MainController::OnPlaybackStateChanged(int state) {
    if (this->pauseButton) {
        switch (state) {
            case musik::core::sdk::PlaybackPaused:
                this->pauseButton->SetCaption(_T("resume")); break;
            case musik::core::sdk::PlaybackPlaying:
                this->pauseButton->SetCaption(_T("pause")); break;
        }

        this->Layout();
    }
}

