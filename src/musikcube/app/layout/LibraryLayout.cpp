//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <stdafx.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <core/library/LocalLibraryConstants.h>
#include <core/runtime/Message.h>
#include <core/support/Messages.h>
#include <core/support/PreferenceKeys.h>

#include <app/overlay/BrowseOverlays.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <core/support/Playback.h>

#include "LibraryLayout.h"

using namespace musik::core::library::constants;
using namespace musik::core::runtime;

#define SHOULD_REFOCUS(target) \
    (this->visibleLayout == target) && \
    (this->shortcuts && !this->shortcuts->IsFocused())

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::cube;
using namespace musik::core::runtime;
using namespace cursespp;

namespace type {
    const std::string CategoryFilter = "CategoryFilter";
    const std::string TrackFilter = "TrackFilter";
    const std::string Browse = "Browse";
    const std::string Directory = "Directory";
    const std::string NowPlaying = "NowPlaying";
};

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

#define REMEMBER(key, value) { \
    auto prefs = Preferences::ForComponent(components::Session); \
    prefs->SetString(key, value.c_str()); this->prefs->Save(); \
}

LibraryLayout::LibraryLayout(musik::core::audio::PlaybackService& playback, ILibraryPtr library)
: LayoutBase()
, playback(playback)
, shortcuts(nullptr)
, transport(playback.GetTransport()) {
    this->library = library;
    this->prefs = Preferences::ForComponent(components::Settings);
    this->InitializeWindows();
}

LibraryLayout::~LibraryLayout() {
}

void LibraryLayout::OnLayout() {
    bool autoHideCommandBar = this->prefs->GetBool(keys::AutoHideCommandBar, false);
    int x = 0, y = 0;
    int cx = this->GetWidth(), cy = this->GetHeight();
#ifdef WIN32
    int transportCy = 3;
#else
    int transportCy = (autoHideCommandBar ? 2 : 3);
#endif
    int mainHeight = cy - transportCy;
    this->transportView->MoveAndResize(1, mainHeight, cx - 2, transportCy);
    if (this->visibleLayout) {
        this->visibleLayout->MoveAndResize(x, y, cx, mainHeight);
        this->visibleLayout->Show();
    }
}

void LibraryLayout::ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout) {
    if (this->visibleLayout != newLayout) {
        this->transportView->SetFocus(TransportWindow::FocusNone);

        if (this->visibleLayout) {
           this->RemoveWindow(this->visibleLayout);
           this->visibleLayout->FocusTerminated.disconnect(this);
           this->visibleLayout->Hide();
        }

        this->visibleLayout = newLayout;

        this->visibleLayout->FocusTerminated.connect(
            this, &LibraryLayout::OnMainLayoutFocusTerminated);

        this->AddWindow(this->visibleLayout);
        this->Layout();

        /* ask the visible layout to terminate focusing, not do it
        in a circular fashion. when we hit the end, we'll focus
        the transport! see FocusNext() and FocusPrev(). */
        this->visibleLayout->SetFocusMode(ILayout::FocusModeTerminating);

        if (this->IsVisible()) {
            this->visibleLayout->BringToTop();
        }

        this->OnLayoutChanged();
    }
}

void LibraryLayout::OnLayoutChanged() {
    this->UpdateShortcutsWindow();
}

void LibraryLayout::ShowNowPlaying() {
    this->ChangeMainLayout(this->nowPlayingLayout);
    REMEMBER(keys::LastLibraryView, type::NowPlaying);
}

void LibraryLayout::ShowBrowse(const std::string& category) {
    this->ChangeMainLayout(this->browseLayout);
    if (category.size()) {
        this->browseLayout->SwitchCategory(category);
    }

    REMEMBER(keys::LastLibraryView, type::Browse);
}

void LibraryLayout::ShowCategorySearch() {
    SHOULD_REFOCUS(this->categorySearchLayout)
        ? this->categorySearchLayout->FocusInput()
        : this->ChangeMainLayout(this->categorySearchLayout);

    REMEMBER(keys::LastLibraryView, type::CategoryFilter);
}

void LibraryLayout::ShowTrackSearch() {
    SHOULD_REFOCUS(this->trackSearchLayout)
        ? this->trackSearchLayout->FocusInput()
        : this->ChangeMainLayout(this->trackSearchLayout);

    REMEMBER(keys::LastLibraryView, type::TrackFilter);
}

void LibraryLayout::ShowDirectories(const std::string& directory) {
    this->directoryLayout->SetDirectory(directory);
    this->ChangeMainLayout(this->directoryLayout);

    REMEMBER(keys::LastLibraryView, type::Directory);
    REMEMBER(keys::LastBrowseDirectoryRoot, directory);
}

void LibraryLayout::InitializeWindows() {
    this->browseLayout.reset(new BrowseLayout(this->playback, this->library));
    this->directoryLayout.reset(new DirectoryLayout(this->playback, this->library));
    this->nowPlayingLayout.reset(new NowPlayingLayout(this->playback, this->library));
    this->categorySearchLayout.reset(new CategorySearchLayout(this->playback, this->library));
    this->categorySearchLayout->SearchResultSelected.connect(this, &LibraryLayout::OnCategorySearchResultSelected);
    this->trackSearchLayout.reset(new TrackSearchLayout(this->playback, this->library));
    this->transportView.reset(new TransportWindow(this->library, this->playback));

    this->AddWindow(this->transportView);
    this->LoadLastSession();
}

void LibraryLayout::LoadLastSession() {
    if (this->prefs->GetBool(musik::core::prefs::keys::SaveSessionOnExit, true)) {
        auto session = Preferences::ForComponent(components::Session);
        const std::string type = session->GetString(keys::LastLibraryView, type::Browse);
        if (type == type::Directory) {
            const std::string lastDirectoryRoot =
                session->GetString(keys::LastBrowseDirectoryRoot, "");

            std::vector<std::string> paths;
            this->library->Indexer()->GetPaths(paths);
            for (auto p : paths) {
                if (p == lastDirectoryRoot) {
                    this->ShowDirectories(p);
                    return;
                }
            }

            this->ShowBrowse();
        }
        else if (type == type::CategoryFilter) {
            this->ShowCategorySearch();
            this->categorySearchLayout->LoadLastSession();
        }
        else if (type == type::TrackFilter) {
            this->ShowTrackSearch();
            this->trackSearchLayout->LoadLastSession();
        }
        else if (type == type::NowPlaying) {
            this->ShowNowPlaying();
        }
        else /*if (type == type::Browse)*/ {
            this->ShowBrowse();
            this->browseLayout->LoadLastSession();
        }
    }
    else {
        this->ShowBrowse();
    }
}

void LibraryLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    this->shortcuts = shortcuts;

    if (this->shortcuts) {
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryBrowse), _TSTR("shortcuts_browse"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryFilter), _TSTR("shortcuts_filter"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryTracks), _TSTR("shortcuts_tracks"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue), _TSTR("shortcuts_play_queue"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        this->shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        this->shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->Broadcast(message::JumpToSettings);
            }
            else if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            else {
                this->KeyPress(key);
            }
        });

        this->UpdateShortcutsWindow();
    }
}

void LibraryLayout::UpdateShortcutsWindow() {
    if (this->shortcuts) {
        if (this->shortcuts->IsFocused() && this->visibleLayout) {
            this->visibleLayout->SetFocus(IWindowPtr());
        }
        if (this->visibleLayout == this->browseLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryBrowse));
        }
        else if (this->visibleLayout == nowPlayingLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue));
        }
        else if (this->visibleLayout == categorySearchLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryFilter));
        }
        else if (this->visibleLayout == trackSearchLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryTracks));
        }
    }
}

void LibraryLayout::OnAddedToParent(IWindow* parent) {
    MessageQueue().RegisterForBroadcasts(this->shared_from_this());
}

void LibraryLayout::OnRemovedFromParent(IWindow* parent) {
    MessageQueue().UnregisterForBroadcasts(this);
}

void LibraryLayout::OnCategorySearchResultSelected(
    CategorySearchLayout* layout, std::string fieldType, int64_t fieldId)
{
    this->ShowBrowse();
    this->browseLayout->ScrollTo(fieldType, fieldId);
}

IWindowPtr LibraryLayout::FocusNext() {
    if (this->transportView->IsFocused()) {
        if (this->transportView->FocusNext()) {
            return this->transportView;
        }

        return this->visibleLayout->FocusFirst();
    }

    return this->visibleLayout->FocusNext();
}

IWindowPtr LibraryLayout::FocusPrev() {
    if (this->transportView->IsFocused()) {
        if (this->transportView->FocusPrev()) {
            return this->transportView;
        }

        return this->visibleLayout->FocusLast();
    }

    return this->visibleLayout->FocusPrev();
}

void LibraryLayout::OnMainLayoutFocusTerminated(
    LayoutBase::FocusDirection direction)
{
    if (direction == LayoutBase::FocusForward) {
        this->transportView->FocusFirst();
    }
    else {
        this->transportView->FocusLast();
    }
}

IWindowPtr LibraryLayout::GetFocus() {
    if (this->transportView->IsFocused()) {
        return this->transportView;
    }

    auto result = this->visibleLayout->GetFocus();

    if (!result) {
        this->visibleLayout->SetFocusIndex(0);
        result = this->visibleLayout->GetFocus();
    }

     return result;
}

bool LibraryLayout::SetFocus(cursespp::IWindowPtr window) {
    if (window == this->transportView) {
        this->transportView->RestoreFocus();
        return true;
    }

    return this->visibleLayout->SetFocus(window);
}

void LibraryLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    switch (message.Type()) {
        case message::JumpToCategory: {
            static std::map<int, const char*> JUMP_TYPE_TO_COLUMN = {
                { cube::message::category::Album, constants::Track::ALBUM },
                { cube::message::category::Artist, constants::Track::ARTIST },
                { cube::message::category::AlbumArtist, constants::Track::ALBUM_ARTIST },
                { cube::message::category::Genre, constants::Track::GENRE }
            };

            auto type = JUMP_TYPE_TO_COLUMN[(int)message.UserData1()];
            auto id = message.UserData2();
            this->OnCategorySearchResultSelected(nullptr, type, id);
        }
        break;

        case core::message::PlaylistModified:
        case core::message::PlaylistCreated:
        case core::message::PlaylistRenamed:
        case core::message::PlaylistDeleted: {
            MessageQueue().Post(Message::Create(
                this->browseLayout.get(),
                message.Type(),
                message.UserData1(),
                message.UserData2()));
        }
        break;
    }

    LayoutBase::ProcessMessage(message);
}

bool LibraryLayout::KeyPress(const std::string& key) {
    if (this->visibleLayout == this->browseLayout ||
        this->visibleLayout == this->directoryLayout)
    {
        if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseArtists, key)) {
            this->ShowBrowse(constants::Track::ARTIST);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbums, key)) {
            this->ShowBrowse(constants::Track::ALBUM);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseGenres, key)) {
            this->ShowBrowse(constants::Track::GENRE);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbumArtists, key)) {
            this->ShowBrowse(constants::Track::ALBUM_ARTIST);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowsePlaylists, key)) {
            this->ShowBrowse(constants::Playlists::TABLE_NAME);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseChooseCategory, key)) {
            BrowseOverlays::ShowCategoryChooser(
                this->library,
                [this](std::string category) {
                    this->ShowBrowse(category);
                });
            return true;
        }
    }

    if (Hotkeys::Is(Hotkeys::NavigateLibraryPlayQueue, key)) {
        this->ShowNowPlaying();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowse, key)) {
        this->ShowBrowse();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryFilter, key)) {
        this->ShowCategorySearch();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryTracks, key)) {
        this->ShowTrackSearch();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseDirectories, key)) {
        BrowseOverlays::ShowDirectoryChooser(
            this->library,
            [this](std::string directory) {
                this->ShowDirectories(directory);
            });
        return true;
    }
    else if (this->GetFocus() == this->transportView && Hotkeys::Is(Hotkeys::Up, key)) {
        this->visibleLayout->FocusLast();
        return true;
    }
    else if (this->GetFocus() == this->transportView && Hotkeys::Is(Hotkeys::Down, key)) {
        this->visibleLayout->FocusFirst();
        return true;
    }
    /* forward to the visible layout */
    else if (this->visibleLayout && this->visibleLayout->KeyPress(key)) {
        return true;
    }
    else if (key == " " || key == "M- ") {
        musik::core::playback::PauseOrResume(this->transport);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
