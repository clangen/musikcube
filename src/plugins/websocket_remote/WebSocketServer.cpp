//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "WebSocketServer.h"
#include "Constants.h"

#include <iostream>

#include <core/sdk/constants.h>

#include <boost/format.hpp>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using namespace nlohmann;
using namespace musik::core::sdk;

static int nextId = 0;

static std::string nextMessageId() {
    return boost::str(boost::format("musikcube-server-%d") % ++nextId);
}

WebSocketServer::WebSocketServer(Context& context)
: context(context)
, running(false) {

}

WebSocketServer::~WebSocketServer() {
    this->Stop();
}

void WebSocketServer::Wait() {
    std::unique_lock<std::mutex> lock(this->exitMutex);
    while (this->running) {
        this->exitCondition.wait(lock);
    }
}

bool WebSocketServer::Start() {
    this->Stop();
    this->running = true;
    this->thread.reset(new std::thread(std::bind(&WebSocketServer::ThreadProc, this)));

    return true;
}

void WebSocketServer::ThreadProc() {
    try {
        wss.reset(new server());

        if (context.prefs->GetBool("debug")) {
            wss->get_alog().set_ostream(&std::cerr);
            wss->get_elog().set_ostream(&std::cerr);
            wss->set_access_channels(websocketpp::log::alevel::all);
            wss->clear_access_channels(websocketpp::log::alevel::frame_payload);
        }
        else {
            wss->set_access_channels(websocketpp::log::alevel::none);
            wss->clear_access_channels(websocketpp::log::alevel::none);
        }

        wss->init_asio();
        wss->set_message_handler(std::bind(&WebSocketServer::OnMessage, this, wss.get(), ::_1, ::_2));
        wss->set_open_handler(std::bind(&WebSocketServer::OnOpen, this, ::_1));
        wss->set_close_handler(std::bind(&WebSocketServer::OnClose, this, ::_1));
        wss->listen(context.prefs->GetInt(prefs::websocket_server_port.c_str(), defaults::websocket_server_port));
        wss->start_accept();

        wss->run();
    }
    catch (websocketpp::exception const & e) {
        std::cerr << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "ThreadProc failed: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "unknown exception" << std::endl;
    }

    this->wss.reset();
    this->running = false;
    this->exitCondition.notify_all();
}

bool WebSocketServer::Stop() {
    if (this->thread) {
        if (this->wss) {
            wss->stop();
        }

        this->thread->join();
        this->thread.reset();
    }

    this->running = false;
    this->exitCondition.notify_all();

    return true;
}

void WebSocketServer::OnTrackChanged(ITrack* track) {
    this->BroadcastPlaybackOverview();
}

void WebSocketServer::OnPlaybackStateChanged(PlaybackState state) {
    this->BroadcastPlaybackOverview();
}

void WebSocketServer::OnPlaybackTimeChanged(double time) {
    this->BroadcastPlaybackOverview();
}

void WebSocketServer::OnVolumeChanged(double volume) {
    this->BroadcastPlaybackOverview();
}

void WebSocketServer::OnModeChanged(RepeatMode repeatMode, bool shuffled) {
    this->BroadcastPlaybackOverview();
}

void WebSocketServer::OnPlayQueueChanged() {
    this->BroadcastPlayQueueChanged();
}

void WebSocketServer::HandleAuthentication(connection_hdl connection, json& request) {
    std::string name = request[message::name];

    if (name == request::authenticate) {
        std::string sent = request[message::options][key::password];

        std::string actual = GetPreferenceString(
            context.prefs, key::password, defaults::password);

        if (sent == actual) {
            this->connections[connection] = true; /* mark as authed */

            this->RespondWithOptions(
                connection,
                request,
                json({ { key::authenticated, true } }));

            return;
        }
    }

    this->wss->close(
        connection,
        websocketpp::close::status::policy_violation,
        value::unauthenticated);
}

void WebSocketServer::HandleRequest(connection_hdl connection, json& request) {
    if (this->connections[connection] == false) {
        this->HandleAuthentication(connection, request);
        return;
    }

    std::string name = request[message::name];
    std::string id = request[message::id];

    if (!name.size() || !id.size()) {
        RespondWithInvalidRequest(connection, name, id);
    }
    else if (context.playback) {
        if (request.find(message::options) == request.end()) {
            request[message::options] = {};
        }

        const json& options = request[message::options];

        if (name == request::ping) {
            this->RespondWithSuccess(connection, request);
            return;
        }
        if (name == request::pause_or_resume) {
            context.playback->PauseOrResume();
            this->RespondWithSuccess(connection, request);
            return;
        }
        else if (name == request::stop) {
            context.playback->Stop();
            this->RespondWithSuccess(connection, request);
            return;
        }
        else if (name == request::previous) {
            if (context.playback->Previous()) {
                this->RespondWithSuccess(connection, request);
            }
            else {
                this->RespondWithFailure(connection, request);
            }
            return;
        }
        else if (name == request::next) {
            if (context.playback->Next()) {
                this->RespondWithSuccess(connection, request);
            }
            else {
                this->RespondWithFailure(connection, request);
            }
            return;
        }
        else if (name == request::play_at_index) {
            if (options.find(key::index) != options.end()) {
                context.playback->Play(options[key::index]);
                this->RespondWithSuccess(connection, request);
            }
            else {
                this->RespondWithFailure(connection, request);
            }
            return;
        }
        else if (name == request::toggle_shuffle) {
            context.playback->ToggleShuffle();
            this->RespondWithSuccess(connection, request);
            return;
        }
        else if (name == request::toggle_repeat) {
            context.playback->ToggleRepeatMode();
            this->RespondWithSuccess(connection, request);
            return;
        }
        else if (name == request::toggle_mute) {
            context.playback->ToggleMute();
            this->RespondWithSuccess(connection, request);
            return;
        }
        else if (name == request::set_volume) {
            this->RespondWithSetVolume(connection, request);
            return;
        }
        else if (name == request::seek_to) {
            if (options.find(key::position) != options.end()) {
                context.playback->SetPosition(options[key::position]);
                this->RespondWithSuccess(connection, request);
            }
            else {
                this->RespondWithFailure(connection, request);
            }
            return;
        }
        else if (name == request::seek_relative) {
            double delta = options.value(key::delta, 0.0f);
            if (delta != 0.0f) {
                context.playback->SetPosition(context.playback->GetPosition() + delta);
                this->RespondWithSuccess(connection, request);
            }
            else {
                this->RespondWithFailure(connection, request);
            }
            return;
        }
        else if (name == request::get_playback_overview) {
            RespondWithPlaybackOverview(connection, request);
            return;
        }
        else if (name == request::query_category) {
            RespondWithQueryCategory(connection, request);
            return;
        }
        else if (name == request::query_tracks) {
            RespondWithQueryTracks(connection, request);
            return;
        }
        else if (name == request::query_tracks_by_category) {
            RespondWithQueryTracksByCategory(connection, request);
            return;
        }
        else if (name == request::query_albums) {
            RespondWithQueryAlbums(connection, request);
            return;
        }
        else if (name == request::query_play_queue_tracks) {
            RespondWithPlayQueueTracks(connection, request);
            return;
        }
        else if (name == request::play_all_tracks) {
            this->RespondWithPlayAllTracks(connection, request);
            return;
        }
        else if (name == request::play_tracks) {
            this->RespondWithPlayTracks(connection, request);
            return;
        }
        else if (name == request::play_tracks_by_category) {
            this->RespondWithPlayTracksByCategory(connection, request);
            return;
        }
        else if (name == request::get_environment) {
            this->RespondWithEnvironment(connection, request);
            return;
        }
        else if (name == request::get_current_time) {
            this->RespondWithCurrentTime(connection, request);
            return;
        }
        else if (name == request::save_playlist) {
            this->RespondWithSavePlaylist(connection, request);
            return;
        }
        else if (name == request::rename_playlist) {
            this->RespondWithRenamePlaylist(connection, request);
            return;
        }
        else if (name == request::delete_playlist) {
            this->RespondWithDeletePlaylist(connection, request);
            return;
        }
    }

    this->RespondWithInvalidRequest(connection, name, id);
}

void WebSocketServer::Broadcast(const std::string& name, json& options) {
    json msg;
    msg[message::name] = name;
    msg[message::type] = type::broadcast;
    msg[message::id] = nextMessageId();
    msg[message::options] = options;

    std::string str = msg.dump();

    auto rl = connectionLock.Read();
    try {
        if (wss) {
            for (const auto &keyValue : this->connections) {
                wss->send(keyValue.first, str.c_str(), websocketpp::frame::opcode::text);
            }
        }
    }
    catch (...) {
        std::cerr << "broadcast failed (stale connection?)\n";
    }
}

void WebSocketServer::RespondWithOptions(connection_hdl connection, json& request, json& options) {
    json response = {
        { message::name, request[message::name] },
        { message::type, type::response },
        { message::id, request[message::id] },
        { message::options, options }
    };

    wss->send(connection, response.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithOptions(connection_hdl connection, json& request, json&& options) {
    json response = {
        { message::name, request[message::name] },
        { message::type, type::response },
        { message::id, request[message::id] },
        { message::options, options }
    };

    wss->send(connection, response.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithInvalidRequest(connection_hdl connection, const std::string& name, const std::string& id)
{
    json error = {
        { message::name, name },
        { message::id, id },
        { message::options,{
            { key::error, value::invalid }
        } }
    };
    wss->send(connection, error.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithSuccess(connection_hdl connection, json& request) {
    std::string name = request[message::name];
    std::string id = request[message::id];
    this->RespondWithSuccess(connection, name, id);
}

void WebSocketServer::RespondWithSuccess(connection_hdl connection, const std::string& name, const std::string& id)
{
    json success = {
        { message::name, name },
        { message::id, id },
        { message::type, type::response },
        { message::options,{ key::success, true } }
    };

    wss->send(connection, success.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithFailure(connection_hdl connection, json& request) {
    json error = {
        { message::name, request[message::name] },
        { message::id, request[message::id] },
        { message::type, type::response },
        { message::options,{ key::success, false } }
    };

    wss->send(connection, error.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithSetVolume(connection_hdl connection, json& request) {
    json& options = request[message::options];
    std::string relative = options.value(key::relative, "");

    if (relative == value::up) {
        double delta = round(context.playback->GetVolume() * 100.0) >= 10.0 ? 0.05 : 0.01;
        context.playback->SetVolume(context.playback->GetVolume() + delta);
    }
    else if (relative == value::down) {
        double delta = round(context.playback->GetVolume() * 100.0) > 10.0 ? 0.05 : 0.01;
        context.playback->SetVolume(context.playback->GetVolume() - delta);
    }
    else if (relative == value::delta) {
        float delta = options[key::volume];
        context.playback->SetVolume(context.playback->GetVolume() + delta);
    }
    else {
        context.playback->SetVolume(options[key::volume]);
    }
}

void WebSocketServer::RespondWithPlaybackOverview(connection_hdl connection, json& request) {
    json options;
    this->BuildPlaybackOverview(options);
    this->RespondWithOptions(connection, request, options);
}

bool WebSocketServer::RespondWithTracks(
    connection_hdl connection,
    json& request,
    ITrackList* tracks,
    int limit,
    int offset)
{
    json& options = request[message::options];
    bool countOnly = options.value(key::count_only, false);
    bool idsOnly = options.value(key::ids_only, false);

    if (tracks) {
        if (countOnly) {
            this->RespondWithOptions(connection, request, {
                { key::data, json::array() },
                { key::count, tracks->Count() }
            });

            tracks->Release();

            return true;
        }
        else {
            json data = json::array();

            IRetainedTrack* track = nullptr;
            for (size_t i = 0; i < tracks->Count(); i++) {
                if (idsOnly) {
                    data.push_back({ {key::id, tracks->GetId(i) } });
                }
                else {
                    track = tracks->GetRetainedTrack(i);
                    data.push_back(this->ReadTrackMetadata(track));
                }

                track->Release();
            }

            tracks->Release();

            this->RespondWithOptions(connection, request, {
                { key::data, data },
                { key::count, data.size() },
                { key::limit, limit },
                { key::offset, offset },
            });

            return true;
        }
    }

    return false;
}

void WebSocketServer::GetLimitAndOffset(json& options, int& limit, int& offset) {
    int optionsLimit = options.value(key::limit, -1);
    int optionsOffset = options.value(key::offset, 0);
    if (optionsLimit != -1 && optionsOffset >= 0) {
        limit = optionsLimit;
        offset = optionsOffset;
    }
}

void WebSocketServer::RespondWithQueryTracks(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        std::string filter = options.value(key::filter, "");

        int limit = -1, offset = 0;
        this->GetLimitAndOffset(options, limit, offset);

        ITrackList* tracks = context.dataProvider->QueryTracks(filter.c_str(), limit, offset);

        if (this->RespondWithTracks(connection, request, tracks, limit, offset)) {
            return;
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithPlayQueueTracks(connection_hdl connection, json& request) {
    /* for the output */
    bool countOnly = false;
    int limit = -1;
    int offset = 0;

    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];
        countOnly = options.value(key::count_only, false);

        if (!countOnly) {
            this->GetLimitAndOffset(options, limit, offset);
        }
    }

    if (countOnly) {
        this->RespondWithOptions(connection, request, {
            { key::data, json::array() },
            { key::count, context.playback->Count() }
        });
    }
    else {
        static auto deleter = [](IRetainedTrack* track) { track->Release(); };
        std::vector<std::shared_ptr<IRetainedTrack>> tracks;

        /* edit the playlist so it can be changed while we're getting the tracks
        out of it. */
        ITrackListEditor* editor = context.playback->EditPlaylist();

        int trackCount = (int)context.playback->Count();
        int to = std::min(trackCount, offset + limit);

        for (int i = offset; i < to; i++) {
            tracks.push_back(std::shared_ptr<IRetainedTrack>(context.playback->GetTrack(i), deleter));
        }

        editor->Release();

        /* now add the tracks to the output. they will be Release()'d automatically
        as soon as this scope ends. */
        json data = json::array();

        for (auto track : tracks) {
            data.push_back(this->ReadTrackMetadata(track.get()));
        }

        this->RespondWithOptions(connection, request, {
            { key::data, data },
            { key::count, data.size() },
            { key::limit, limit },
            { key::offset, offset },
        });
    }
}

void WebSocketServer::RespondWithQueryAlbums(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        std::string filter = options.value(key::filter, "");
        std::string category = options.value(key::category, "");
        uint64_t categoryId = options.value(key::category_id, -1);

        IMetadataMapList* albumList = context.dataProvider
            ->QueryAlbums(category.c_str(), categoryId, filter.c_str());

        json result = json::array();

        IMetadataMap* album;
        for (size_t i = 0; i < albumList->Count(); i++) {
            album = albumList->GetMetadata(i);

            result.push_back({
                { key::title, album->GetDescription() },
                { key::id, album->GetId() },
                { key::thumbnail_id, 0 }, /* note: thumbnails aren't supported at the album level yet */
                { key::album_artist_id, album->GetInt64(key::album_artist_id.c_str()) },
                { key::album_artist, GetMetadataString(album, key::album_artist) }
            });

            album->Release();
        }

        albumList->Release();

        this->RespondWithOptions(connection, request, {
            { key::category, key::album },
            { key::data, result }
        });

        return;
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithPlayTracks(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        if (options.find(key::ids) != options.end()) {
            json& ids = options[key::ids];
            if (ids.is_array()) {
                size_t count = 0;
                ITrackListEditor* editor = context.playback->EditPlaylist();
                editor->Clear();

                for (auto it = ids.begin(); it != ids.end(); ++it) {
                    editor->Add(*it);
                    ++count;
                }

                editor->Release();

                if (count > 0) {
                    context.playback->Play(0);
                    this->RespondWithSuccess(connection, request);
                    return;
                }
            }
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

ITrackList* WebSocketServer::QueryTracksByCategory(json& request, int& limit, int& offset) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        std::string category = options[key::category];
        uint64_t selectedId = options[key::id];

        std::string filter = options.value(key::filter, "");

        limit = -1, offset = 0;
        this->GetLimitAndOffset(options, limit, offset);

        return context.dataProvider->QueryTracksByCategory(
            category.c_str(), selectedId, filter.c_str(), limit, offset);
    }

    return nullptr;
}

void WebSocketServer::RespondWithQueryTracksByCategory(connection_hdl connection, json& request) {
    int limit, offset;
    ITrackList* tracks = QueryTracksByCategory(request, limit, offset);

    if (tracks && this->RespondWithTracks(connection, request, tracks, limit, offset)) {
        return;
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithQueryCategory(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        std::string category = request[message::options][key::category];
        std::string filter = request[message::options].value(key::filter, "");

        if (category.size()) {
            IMetadataValueList* result = context.dataProvider
                ->QueryCategory(category.c_str(), filter.c_str());

            if (result != nullptr) {
                json list = json::array();

                IMetadataValue* current;
                for (size_t i = 0; i < result->Count(); i++) {
                    current = result->GetAt(i);

                    list[i] = {
                        { key::id, current->GetId() },
                        { key::value, current->GetValue() }
                    };
                }

                result->Release();

                this->RespondWithOptions(connection, request, {
                    { key::category, category },
                    { key::data, list }
                });

                return;
            }
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithPlayAllTracks(connection_hdl connection, json& request) {
    size_t index = 0;
    std::string filter;

    if (request.find(message::options) != request.end()) {
        index = request[message::options].value(key::index, 0);
        filter = request[message::options].value(key::filter, "");
    }

    ITrackList* tracks = context.dataProvider->QueryTracks(filter.c_str());

    if (tracks) {
        context.playback->Play(tracks, index);
        tracks->Release();
    }

    RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithPlayTracksByCategory(connection_hdl connection, json& request) {
    int limit, offset;
    ITrackList* tracks = this->QueryTracksByCategory(request, limit, offset);

    if (tracks) {
        size_t index = request[message::options].value(key::index, 0);
        context.playback->Play(tracks, index);
        tracks->Release();
    }

    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithEnvironment(connection_hdl connection, json& request) {
    this->RespondWithOptions(connection, request, {
        { prefs::http_server_enabled, context.prefs->GetBool(prefs::http_server_enabled.c_str()) },
        { prefs::http_server_port, context.prefs->GetInt(prefs::http_server_port.c_str()) }
    });
}

void WebSocketServer::RespondWithCurrentTime(connection_hdl connection, json& request) {
    auto track = context.playback->GetPlayingTrack();

    this->RespondWithOptions(connection, request, {
        { key::playing_current_time, context.playback->GetPosition() },
        { key::id, track ? track->GetId() : -1 }
    });
}

void WebSocketServer::RespondWithSavePlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];

    json& ids = options[key::ids];
    if (ids.is_array()) {
        int64_t id = options.value(key::playlist_id, 0);
        std::string name = options.value(key::playlist_name, "");

        size_t count = ids.size();
        int64_t* idArray = new int64_t[count];
        std::copy(ids.begin(), ids.end(), idArray);

        uint64_t newPlaylistId = this->context.dataProvider
            ->SavePlaylist(idArray, count, name.c_str(), id);

        delete[] idArray;

        if (newPlaylistId != 0) {
            this->RespondWithOptions(connection, request, {
                { key::playlist_id, newPlaylistId }
            });
        }
        else {
            this->RespondWithFailure(connection, request);
        }
    }
    else {
        this->RespondWithInvalidRequest(
            connection, request[message::name], request[message::id]);
    }
}

void WebSocketServer::RespondWithRenamePlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    int64_t id = options[key::playlist_id];
    std::string name = options[key::playlist_name];

    this->context.dataProvider->RenamePlaylist(id, name.c_str())
        ? this->RespondWithSuccess(connection, request)
        : this->RespondWithFailure(connection, request);
}

void WebSocketServer::RespondWithDeletePlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    int64_t id = options[key::playlist_id];

    this->context.dataProvider->DeletePlaylist(id)
        ? this->RespondWithSuccess(connection, request)
        : this->RespondWithFailure(connection, request);
}

void WebSocketServer::BroadcastPlaybackOverview() {
    {
        auto rl = connectionLock.Read();
        if (!this->connections.size()) {
            return;
        }
    }

    json options;
    this->BuildPlaybackOverview(options);
    this->Broadcast(broadcast::playback_overview_changed, options);
}

void WebSocketServer::BroadcastPlayQueueChanged() {
    {
        auto rl = connectionLock.Read();
        if (!this->connections.size()) {
            return;
        }
    }

    json options;
    this->Broadcast(broadcast::play_queue_changed, options);
}

json WebSocketServer::WebSocketServer::ReadTrackMetadata(IRetainedTrack* track) {
    return {
        { key::id, track->GetId() },
        { key::external_id, GetMetadataString(track, key::external_id) },
        { key::title, GetMetadataString(track, key::title) },
        { key::track_num, track->GetInt32(key::track_num.c_str(), 0) },
        { key::album, GetMetadataString(track, key::album) },
        { key::album_id, track->GetInt64(key::album_id.c_str()) },
        { key::album_artist, GetMetadataString(track, key::album_artist) },
        { key::album_artist_id, track->GetInt64(key::album_artist_id.c_str()) },
        { key::artist, GetMetadataString(track, key::artist) },
        { key::artist_id, track->GetInt64(key::artist_id.c_str()) },
        { key::genre, GetMetadataString(track, key::genre) },
        { key::genre_id, track->GetInt64(key::genre_id.c_str()) },
        { key::thumbnail_id, track->GetInt64(key::thumbnail_id.c_str()) },
    };
}

void WebSocketServer::BuildPlaybackOverview(json& options) {
    options[key::state] = PLAYBACK_STATE_TO_STRING.left.find(context.playback->GetPlaybackState())->second;
    options[key::repeat_mode] = REPEAT_MODE_TO_STRING.left.find(context.playback->GetRepeatMode())->second;
    options[key::volume] = context.playback->GetVolume();
    options[key::shuffled] = context.playback->IsShuffled();
    options[key::muted] = context.playback->IsMuted();
    options[key::play_queue_count] = context.playback->Count();
    options[key::play_queue_position] = context.playback->GetIndex();
    options[key::playing_duration] = context.playback->GetDuration();
    options[key::playing_current_time] = context.playback->GetPosition();

    IRetainedTrack* track = context.playback->GetPlayingTrack();
    if (track) {
        options[key::playing_track] = this->ReadTrackMetadata(track);
        track->Release();
    }
}

void WebSocketServer::OnOpen(connection_hdl connection) {
    auto wl = connectionLock.Write();
    connections[connection] = false;
}

void WebSocketServer::OnClose(connection_hdl connection) {
    auto wl = connectionLock.Write();
    connections.erase(connection);
}

void WebSocketServer::OnMessage(server* s, connection_hdl hdl, message_ptr msg) {
    auto rl = connectionLock.Read();

    try {
        json data = json::parse(msg->get_payload());
        std::string type = data[message::type];
        if (type == type::request) {
            this->HandleRequest(hdl, data);
        }
    }
    catch (std::exception& e) {
        std::cerr << "OnMessage failed: " << e.what() << std::endl;
        this->RespondWithInvalidRequest(hdl, value::invalid, value::invalid);
    }
    catch (...) {
        std::cerr << "message parse failed: " << msg->get_payload() << "\n";
        this->RespondWithInvalidRequest(hdl, value::invalid, value::invalid);
    }
}