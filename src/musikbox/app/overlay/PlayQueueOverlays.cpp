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

#include "stdafx.h"

#include "PlayQueueOverlays.h"

#include <core/audio/Visualizer.h>
#include <core/library/LocalLibraryConstants.h>

#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/CategoryListQuery.h>
#include <core/library/query/local/GetPlaylistQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
#include <core/library/query/local/DeletePlaylistQuery.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>

#define DEFAULT_OVERLAY_WIDTH 30

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::box;
using namespace cursespp;

using Adapter = cursespp::SimpleScrollAdapter;

static std::shared_ptr<Adapter> createAddToAdapter() {
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry("add to end");
    adapter->AddEntry("add as next");
    adapter->SetSelectable(true);
    return adapter;
}

static std::shared_ptr<CategoryListQuery> queryPlaylists(musik::core::ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query(
        new CategoryListQuery(Playlists::TABLE_NAME, ""));

    library->Enqueue(query, ILibrary::QuerySynchronous);
    if (query->GetStatus() != IQuery::Finished) {
        query.reset();
        return query;
    }

    return query;
}

static void addPlaylistsToAdapter(
    std::shared_ptr<Adapter> adapter, CategoryListQuery::ResultList result)
{
    auto it = result->begin();
    while (it != result->end()) {
        adapter->AddEntry((*it)->displayValue);
        ++it;
    }
}

static void showPlaylistListOverlay(
        const std::string& title,
        std::shared_ptr<Adapter> adapter,
        ListOverlay::ItemSelectedCallback callback,
        size_t selectedIndex = 0)
{
    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(title)
        .SetWidth(DEFAULT_OVERLAY_WIDTH)
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(callback);

    cursespp::App::Overlays().Push(dialog);
}

static void confirmOverwritePlaylist(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    const DBID playlistId,
    std::shared_ptr<TrackList> tracks)
{
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle("musikbox")
        .SetMessage("are you sure you want to overwrite the playlist '" + playlistName + "'?")
        .AddButton("^[", "ESC", "no")
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            "yes",
            [library, playlistId, tracks](const std::string& str) {
                library->Enqueue(SavePlaylistQuery::Replace(playlistId, tracks));
            });

    App::Overlays().Push(dialog);
}

static void createNewPlaylist(
    std::shared_ptr<TrackList> tracks,
    musik::core::ILibraryPtr library)
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle("playlist name")
        .SetWidth(DEFAULT_OVERLAY_WIDTH)
        .SetText("")
        .SetInputAcceptedCallback(
            [tracks, library](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Save(name, tracks));
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void renamePlaylist(
    musik::core::ILibraryPtr library,
    const DBID playlistId,
    const std::string& oldName)
{
    std::shared_ptr<InputOverlay> dialog(new InputOverlay());

    dialog->SetTitle("new playlist name")
        .SetWidth(DEFAULT_OVERLAY_WIDTH)
        .SetText(oldName)
        .SetInputAcceptedCallback(
            [library, playlistId](const std::string& name) {
                if (name.size()) {
                    library->Enqueue(SavePlaylistQuery::Rename(playlistId, name));
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

static void confirmDeletePlaylist(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    const DBID playlistId)
{
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle("musikbox")
        .SetMessage("are you sure you want to delete '" + playlistName + "'?")
        .AddButton("^[", "ESC", "no")
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            "yes",
            [library, playlistId](const std::string& str) {
                library->Enqueue(std::shared_ptr<DeletePlaylistQuery>(
                    new DeletePlaylistQuery(playlistId)));
            });

    App::Overlays().Push(dialog);
}

static void showNoPlaylistsDialog() {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle("musikbox")
        .SetMessage("you haven't saved any playlists!")
        .AddButton("KEY_ENTER", "ENTER", "ok");

    App::Overlays().Push(dialog);
}

PlayQueueOverlays::PlayQueueOverlays() {
}

void PlayQueueOverlays::ShowAddTrackOverlay(
    PlaybackService& playback,
    TrackListView& trackList)
{
    size_t selectedIndex = trackList.GetSelectedIndex();

    if (selectedIndex == ListWindow::NO_SELECTION) {
        return;
    }

    DBID trackId = trackList.Get(selectedIndex)->Id();

    auto adapter = createAddToAdapter();

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle("add to play queue")
        .SetSelectedIndex(0)
        .SetWidth(DEFAULT_OVERLAY_WIDTH)
        .SetItemSelectedCallback(
            [trackId, &playback](cursespp::IScrollAdapterPtr adapter, size_t index) {
                auto editor = playback.Edit();
                if (index == 0) { /* end */
                    editor.Add(trackId);
                }
                else { /* next */
                    size_t position = playback.GetIndex();
                    if (position == ListWindow::NO_SELECTION) {
                        editor.Add(trackId);
                    }
                    else {
                        editor.Insert(trackId, position + 1);
                    }
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowAddCategoryOverlay(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    const std::string& fieldColumn,
    DBID fieldId)
{
    auto adapter = createAddToAdapter();

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle("add to play queue")
        .SetSelectedIndex(0)
        .SetWidth(DEFAULT_OVERLAY_WIDTH)
        .SetItemSelectedCallback(
            [&playback, library, fieldColumn, fieldId]
            (cursespp::IScrollAdapterPtr adapter, size_t index) {
                if (index == ListWindow::NO_SELECTION) {
                    return;
                }

                std::shared_ptr<CategoryTrackListQuery>
                    query(new CategoryTrackListQuery(
                        library,
                        fieldColumn,
                        fieldId));

                library->Enqueue(query, ILibrary::QuerySynchronous);

                if (query->GetStatus() == IQuery::Finished) {
                    auto editor = playback.Edit();
                    auto tracks = query->GetResult();
                    size_t position = playback.GetIndex();

                    if (index == 0 || position == ListWindow::NO_SELECTION) { /* end */
                        for (size_t i = 0; i < tracks->Count(); i++) {
                            editor.Add(tracks->GetId(i));
                        }
                    }
                    else { /* after next */
                        for (size_t i = 0; i < tracks->Count(); i++) {
                            editor.Insert(tracks->GetId(i), position + 1 + i);
                        }
                    }
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlayQueueOverlays::ShowLoadPlaylistOverlay(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    PlaylistSelectedCallback callback)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        "load playlist",
        adapter,
        [library, result, callback]
        (cursespp::IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION && callback) {
                DBID playlistId = (*result)[index]->id;
                callback(playlistId);
            }
        });
}

void PlayQueueOverlays::ShowSavePlaylistOverlay(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library,
    DBID selectedPlaylistId)
{
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    adapter->AddEntry("new...");
    addPlaylistsToAdapter(adapter, result);

    /* the caller can specify a playlistId that we should try to
    select by default. if they did, try to find it */
    size_t selectedIndex = 0;
    if (selectedPlaylistId != -1) {
        for (size_t i = 0; i < result->size(); i++) {
            if (result->at(i)->id == selectedPlaylistId) {
                selectedIndex = i + 1; /* offset "new..." */
                break;
            }
        }
    }

    showPlaylistListOverlay(
        "save playlist",
        adapter,
        [&playback, library, result]
        (cursespp::IScrollAdapterPtr adapter, size_t index) {
            std::shared_ptr<TrackList> tracks(new TrackList(library));
            playback.CopyTo(*tracks);

            if (index == 0) { /* create new */
                createNewPlaylist(tracks, library);
            }
            else { /* replace existing */
                --index;
                DBID playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                confirmOverwritePlaylist(library, playlistName, playlistId, tracks);
            }
        },
        selectedIndex);
}

void PlayQueueOverlays::ShowRenamePlaylistOverlay(musik::core::ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        "rename playlist",
        adapter,
        [library, result](cursespp::IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                DBID playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                renamePlaylist(library, playlistId, playlistName);
            }
        });
}

void PlayQueueOverlays::ShowDeletePlaylistOverlay(musik::core::ILibraryPtr library) {
    std::shared_ptr<CategoryListQuery> query = queryPlaylists(library);
    auto result = query->GetResult();

    if (!result->size()) {
        showNoPlaylistsDialog();
        return;
    }

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->SetSelectable(true);
    addPlaylistsToAdapter(adapter, result);

    showPlaylistListOverlay(
        "delete playlist",
        adapter,
        [library, result]
        (cursespp::IScrollAdapterPtr adapter, size_t index) {
            if (index != ListWindow::NO_SELECTION) {
                DBID playlistId = (*result)[index]->id;
                std::string playlistName = (*result)[index]->displayValue;
                confirmDeletePlaylist(library, playlistName, playlistId);
            }
        });
}