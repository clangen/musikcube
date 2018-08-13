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

/* UTILITY METHODS */

static std::string nextMessageId() {
    return boost::str(boost::format("musikcube-server-%d") % ++nextId);
}

static std::shared_ptr<char*> jsonToStringArray(const json& jsonArray) {
    char** result = nullptr;
    size_t count = 0;

    if (jsonArray.is_array()) {
        count = jsonArray.size();
        result = (char**) malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; i++) {
            std::string str = jsonArray[i];
            size_t size = str.size();
            result[i] = (char*) malloc(size + 1);
            strncpy(result[i], str.c_str(), size);
            result[i][size] = 0;
        }
    }

    return std::shared_ptr<char*>(result, [count](char** result) {
        if (result) {
            for (size_t i = 0; i < count; i++) {
                free(result[i]);
            }
            free(result);
        }
    });
}

template <typename T>
static std::shared_ptr<T> jsonToIntArray(json& arr) {
    size_t count = arr.is_array() ? arr.size() : 0;

    T* idArray = new T[count];
    if (count > 0) {
        std::copy(arr.begin(), arr.end(), idArray);
    }

    return std::shared_ptr<T>(idArray, [count](T* result) {
        delete[] result;
    });
}

static std::shared_ptr<IValue*> jsonToPredicateList(json& arr) {
    size_t count = arr.is_array() ? arr.size() : 0;
    IValue** valueArray = new IValue*[count];

    for (size_t i = 0; i < count; i++) {
        valueArray[i] = CreateValue(
            arr[i]["category"],
            arr[i]["id"],
            "category");
    }

    return std::shared_ptr<IValue*>(valueArray, [count](IValue** del) {
        for (size_t i = 0; i < count; i++) {
            del[i]->Release();
        }
        delete[] del;
    });
}

static json getEnvironment(Context& context) {
    return {
        { prefs::http_server_enabled, context.prefs->GetBool(prefs::http_server_enabled.c_str()) },
        { prefs::http_server_port, context.prefs->GetInt(prefs::http_server_port.c_str()) },
        { key::sdk_version, musik::core::sdk::SdkVersion },
        { key::api_version, ApiVersion }
    };
}

/* IMPLEMENTATION */

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

        using namespace websocketpp::lib::asio::ip;

        const int port = context.prefs->GetInt(
            prefs::websocket_server_port.c_str(), defaults::websocket_server_port);

        const bool ipv6 = context.prefs->GetBool(
            prefs::use_ipv6.c_str(), defaults::use_ipv6);

        wss->init_asio();
        wss->set_reuse_addr(true);
        wss->set_message_handler(std::bind(&WebSocketServer::OnMessage, this, wss.get(), ::_1, ::_2));
        wss->set_open_handler(std::bind(&WebSocketServer::OnOpen, this, ::_1));
        wss->set_close_handler(std::bind(&WebSocketServer::OnClose, this, ::_1));
        wss->listen(ipv6 ? tcp::v6() : tcp::v4(), port);
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
    this->snapshots.Reset();

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
                connection, request, json({
                    { key::authenticated, true },
                    { key::environment, getEnvironment(context) }
                }));

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

    auto deviceId = request.value(message::device_id, "");
    if (deviceId.size() == 0) {
        deviceId = "default_device_id";
        request[message::device_id] = deviceId;
    }

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
        else if (name == request::list_categories) {
            RespondWithListCategories(connection, request);
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
        else if (name == request::query_tracks_by_external_ids) {
            RespondWithQueryTracksByExternalIds(connection, request);
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
        else if (name == request::play_snapshot_tracks) {
            this->RespondWithPlaySnapshotTracks(connection, request);
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
        else if (name == request::append_to_playlist) {
            this->RespondWithAppendToPlaylist(connection, request);
            return;
        }
        else if (name == request::remove_tracks_from_playlist) {
            this->RespondWithRemoveTracksFromPlaylist(connection, request);
            return;
        }
        else if (name == request::run_indexer) {
            this->RespondWithRunIndexer(connection, request);
            return;
        }
        else if (name == request::list_output_drivers) {
            this->RespondWithListOutputDrivers(connection, request);
            return;
        }
        else if (name == request::set_default_output_driver) {
            this->RespondWithSetDefaultOutputDriver(connection, request);
            return;
        }
        else if (name == request::get_gain_settings) {
            this->RespondWithGetGainSettings(connection, request);
            return;
        }
        else if (name == request::set_gain_settings) {
            this->RespondWithSetGainSettings(connection, request);
            return;
        }
        else if (name == request::get_transport_type) {
            this->RespondWithGetTransportType(connection, request);
            return;
        }
        else if (name == request::set_transport_type) {
            this->RespondWithSetTransportType(connection, request);
            return;
        }
        else if (name == request::snapshot_play_queue) {
            this->RespondWithSnapshotPlayQueue(connection, request);
            return;
        }
        else if (name == request::invalidate_play_queue_snapshot) {
            this->snapshots.Remove(deviceId);
            this->RespondWithSuccess(connection, request);
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
        { message::type, type::response },
        { message::options,{{ key::error, value::invalid }} }
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
        { message::options, {{ key::success, true }} }
    };

    wss->send(connection, success.dump().c_str(), websocketpp::frame::opcode::text);
}

void WebSocketServer::RespondWithFailure(connection_hdl connection, json& request) {
    json error = {
        { message::name, request[message::name] },
        { message::id, request[message::id] },
        { message::type, type::response },
        { message::options, {{ key::success, false }} }
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

    this->RespondWithSuccess(connection, request);
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

            ITrack* track = nullptr;
            for (size_t i = 0; i < tracks->Count(); i++) {
                track = tracks->GetTrack(i);

                if (idsOnly) {
                    data.push_back(GetMetadataString(track, key::external_id));
                }
                else {
                    data.push_back(this->ReadTrackMetadata(track));
                }

                track->Release();
            }

            tracks->Release();

            this->RespondWithOptions(connection, request, {
                { key::data, data },
                { key::count, data.size() },
                { key::limit, std::max(0, limit) },
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

ITrackList* WebSocketServer::QueryTracks(json& request, int& limit, int& offset) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];
        std::string filter = options.value(key::filter, "");
        this->GetLimitAndOffset(options, limit, offset);
        return context.dataProvider->QueryTracks(filter.c_str(), limit, offset);
    }
    return nullptr;
}

void WebSocketServer::RespondWithQueryTracks(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        int limit = -1, offset = 0;
        ITrackList* tracks = this->QueryTracks(request, limit, offset);
        if (this->RespondWithTracks(connection, request, tracks, limit, offset)) {
            return;
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithQueryTracksByExternalIds(connection_hdl connection, json& request) {
    auto& options = request[message::options];

    if (options.find(key::external_ids) != options.end()) {
        json& externalIds = options[key::external_ids];
        if (externalIds.is_array()) {
            auto externalIdArray = jsonToStringArray(externalIds);
            ITrackList* trackList = context.dataProvider
                ->QueryTracksByExternalId(
                    (const char**) externalIdArray.get(),
                    externalIds.size());

            if (trackList) {
                json tracks = { };

                ITrack* track;
                std::string externalId;
                for (size_t i = 0; i < trackList->Count(); i++) {
                    track = trackList->GetTrack(i);
                    externalId = GetMetadataString(track, track::ExternalId);
                    tracks[externalId] = this->ReadTrackMetadata(track);
                    track->Release();
                }

                trackList->Release();
                json options = { { key::data, tracks } };

                this->RespondWithOptions(connection, request, options);
                return;
            }
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}


void WebSocketServer::RespondWithPlayQueueTracks(connection_hdl connection, json& request) {
    /* for the output */
    bool countOnly = false;
    int limit = -1;
    int offset = 0;

    /* note: the user can query the "live" (i.e. current) play queue, or a
    a previously "snapshotted" playqueue. the former is generally used for
    remote playback, the latter is used for transfering context from remote
    to streaming. */
    std::string type = value::live;

    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];
        countOnly = options.value(key::count_only, false);
        type = options.value(key::type, type);

        if (!countOnly) {
            this->GetLimitAndOffset(options, limit, offset);
        }
    }

    if (countOnly) {
        size_t count = context.playback->Count();

        if (type == value::snapshot) {
            auto snapshot = snapshots.Get(request[message::device_id]);
            count = snapshot ? snapshot->Count() : 0;
        }

        this->RespondWithOptions(connection, request, {
            { key::data, json::array() },
            { key::count, count }
        });
    }
    else {
        bool idsOnly = request[message::options].value(key::ids_only, false);

        /* now add the tracks to the output. they will be Release()'d automatically
        as soon as this scope ends. */
        json data = json::array();

        if (type == value::live) {
            /* edit the playlist so it can be changed while we're getting the tracks
            out of it. only applicable for the "live" type. */
            ITrackListEditor* editor = context.playback->EditPlaylist();
            int to = (int)context.playback->Count();

            if (offset >= 0 && limit >= 0) {
                to = std::min(to, offset + limit);
            }

            for (int i = offset; i < to; i++) {
                ITrack* track = context.playback->GetTrack(i);
                if (idsOnly) {
                    data.push_back(GetMetadataString(track, key::external_id));
                }
                else {
                    data.push_back(this->ReadTrackMetadata(track));
                }
                if (track) {
                    track->Release();
                }
            }

            editor->Release();
        }
        else if (type == value::snapshot) {
            auto snapshot = snapshots.Get(request[message::device_id]);
            if (snapshot) {
                int to = (int) snapshot->Count();

                if (offset >= 0 && limit >= 0) {
                    to = std::min(to, offset + limit);
                }

                for (int i = offset; i < to; i++) {
                    ITrack* track = snapshot->GetTrack(i);
                    if (idsOnly) {
                        data.push_back(GetMetadataString(track, key::external_id));
                    }
                    else {
                        data.push_back(this->ReadTrackMetadata(track));
                    }
                    if (track) {
                        track->Release();
                    }
                }
            }
        }

        this->RespondWithOptions(connection, request, {
            { key::data, data },
            { key::count, data.size() },
            { key::limit, std::max(0, limit) },
            { key::offset, offset },
        });
    }
}

void WebSocketServer::RespondWithQueryAlbums(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        std::string filter = options.value(key::filter, "");
        std::string category = options.value(key::category, "");
        int64_t categoryId = options.value<int64_t>(key::category_id, -1);

        IMapList* albumList = context.dataProvider
            ->QueryAlbums(category.c_str(), categoryId, filter.c_str());

        json result = json::array();

        IMap* album;
        for (size_t i = 0; i < albumList->Count(); i++) {
            album = albumList->GetAt(i);

            result.push_back({
                { key::title, GetValueString(album) },
                { key::id, album->GetId() },
                { key::thumbnail_id, album->GetInt64(key::thumbnail_id.c_str()) },
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
    bool success = false;

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

                success = true;
                editor->Release();
            }
        }
        else if (options.find(key::external_ids) != options.end()) {
            json& externalIds = options[key::ids];
            if (externalIds.is_array()) {
                auto externalIdArray = jsonToStringArray(externalIds);

                ITrackList* trackList = context.dataProvider
                    ->QueryTracksByExternalId(
                    (const char**)externalIdArray.get(),
                    externalIds.size());

                if (trackList && trackList->Count()) {
                    ITrackListEditor* editor = context.playback->EditPlaylist();
                    editor->Clear();

                    for (size_t i = 0; i < trackList->Count(); i++) {
                        editor->Add(trackList->GetId(i));
                    }

                    editor->Release();
                    trackList->Release();
                    success = true;
                }
            }
        }
    }

    if (success) {
        size_t index = request[message::options].value(key::index, 0);
        double time = request[message::options].value(key::time, 0.0);

        context.playback->Play(index);

        if (time > 0.0) {
            context.playback->SetPosition(time);
        }

        this->RespondWithSuccess(connection, request);
    }
    else {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
    }
}

ITrackList* WebSocketServer::QueryTracksByCategory(json& request, int& limit, int& offset) {
    if (request.find(message::options) != request.end()) {
        json& options = request[message::options];

        std::string category = options.value(key::category, "");
        int64_t selectedId = options.value<int64_t>(key::id, -1);
        auto predicates = options.value(key::predicates, json::array());

        std::string filter = options.value(key::filter, "");

        limit = -1, offset = 0;
        this->GetLimitAndOffset(options, limit, offset);

        if (predicates.size()) {
            auto predicateList = jsonToPredicateList(predicates);

            return context.dataProvider->QueryTracksByCategories(
                predicateList.get(), predicates.size(), filter.c_str(), limit, offset);
        }
        else {
            return context.dataProvider->QueryTracksByCategory(
                category.c_str(), selectedId, filter.c_str(), limit, offset);
        }
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

void WebSocketServer::RespondWithListCategories(connection_hdl connection, json& request) {
    IValueList* result = context.dataProvider->ListCategories();

    if (result != nullptr) {
        json list = json::array();

        for (size_t i = 0; i < result->Count(); i++) {
            list[i] = GetValueString(result->GetAt(i));
        }

        result->Release();

        this->RespondWithOptions(connection, request, { { key::data, list } });
    }
    else {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
    }
}

void WebSocketServer::RespondWithQueryCategory(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        auto& options = request[message::options];
        std::string category = options[key::category];
        std::string filter = options.value(key::filter, "");

        /* single predicate */
        std::string predicate = options.value(key::predicate_category, "");
        int64_t predicateId = options.value<int64_t>(key::predicate_id, -1LL);

        /* multiple predicates */
        auto predicates = options.value(key::predicates, json::array());

        if (category.size()) {
            IValueList* result;

            if (predicates.size()) {
                auto predicateList = jsonToPredicateList(predicates);
                result = context.dataProvider
                    ->QueryCategoryWithPredicates(
                        category.c_str(),
                        predicateList.get(),
                        predicates.size(),
                        filter.c_str());
            }
            else {
                result = context.dataProvider
                    ->QueryCategoryWithPredicate(
                        category.c_str(),
                        predicate.c_str(),
                        predicateId,
                        filter.c_str());
            }

            if (result != nullptr) {
                json list = json::array();

                IValue* current;
                for (size_t i = 0; i < result->Count(); i++) {
                    current = result->GetAt(i);

                    list[i] = {
                        { key::id, current->GetId() },
                        { key::value, GetValueString(current) }
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
    double time = 0.0;

    if (request.find(message::options) != request.end()) {
        index = request[message::options].value(key::index, 0);
        filter = request[message::options].value(key::filter, "");
        time = request[message::options].value(key::time, 0.0);
    }

    ITrackList* tracks = context.dataProvider->QueryTracks(filter.c_str());

    if (tracks) {
        context.playback->Play(tracks, index);

        if (time > 0.0) {
            context.playback->SetPosition(time);
        }

        tracks->Release();
    }

    RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithPlaySnapshotTracks(connection_hdl connection, json& request) {
    auto snapshot = this->snapshots.Get(request[message::device_id]);
    if (snapshot) {
        size_t index = 0;
        double time = 0.0;

        if (request.find(message::options) != request.end()) {
            index = request[message::options].value(key::index, 0);
            time = request[message::options].value(key::time, 0.0);
        }

        context.playback->Play(snapshot, index);

        if (time > 0.0) {
            context.playback->SetPosition(time);
        }
    }
    else {
        context.playback->Stop();
        auto editor = context.playback->EditPlaylist();
        editor->Clear();
        editor->Release();
    }

    RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithPlayTracksByCategory(connection_hdl connection, json& request) {
    int limit, offset;
    ITrackList* tracks = this->QueryTracksByCategory(request, limit, offset);

    if (tracks) {
        size_t index = request[message::options].value(key::index, 0);
        double time = request[message::options].value(key::time, 0.0);

        context.playback->Play(tracks, index);

        if (time > 0.0) {
            context.playback->SetPosition(time);
        }

        tracks->Release();
    }

    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithEnvironment(connection_hdl connection, json& request) {
    this->RespondWithOptions(connection, request, getEnvironment(context));
}

void WebSocketServer::RespondWithCurrentTime(connection_hdl connection, json& request) {
    auto track = context.playback->GetPlayingTrack();

    this->RespondWithOptions(connection, request, {
        { key::playing_current_time, context.playback->GetPosition() },
        { key::id, track ? track->GetId() : -1 }
    });
}

void WebSocketServer::RespondWithSavePlaylist(connection_hdl connection, json& request) {
    /* TODO: a lot of copy/paste between this method and RespondWithAppendToPlaylist */

    auto& options = request[message::options];
    int64_t id = options.value<int64_t>(key::playlist_id, 0);
    std::string name = options.value(key::playlist_name, "");

    /* by external id (slower, more reliable) */
    if (options.find(key::external_ids) != options.end()) {
        json& externalIds = options[key::external_ids];

        if (externalIds.is_array()) {
            auto externalIdArray = jsonToStringArray(externalIds);

            int64_t newPlaylistId = this->context.dataProvider
                ->SavePlaylistWithExternalIds(
                    (const char**) externalIdArray.get(),
                    externalIds.size(),
                    name.c_str(),
                    id);

            if (newPlaylistId != 0) {
                this->RespondWithOptions(connection, request, {
                    { key::playlist_id, newPlaylistId }
                });
                return;
            }

            this->RespondWithFailure(connection, request);
            return;
        }
    }
    /* by subquery (query_tracks or query_tracks_by_category) */
    else if (options.find(key::subquery) != options.end()) {
        auto& subquery = options[key::subquery];
        std::string type = subquery.value(key::type, "");

        if (subquery.find(message::options) != subquery.end()) {
            ITrackList* tracks = nullptr;
            int queryLimit, queryOffset;
            if (type == request::query_tracks) {
                tracks = this->QueryTracks(subquery, queryLimit, queryOffset);
            }
            else if (type == request::query_tracks_by_category) {
                tracks = this->QueryTracksByCategory(subquery, queryLimit, queryOffset);
            }

            if (tracks) {
                int64_t newPlaylistId = this->context.dataProvider
                    ->SavePlaylistWithTrackList(tracks, name.c_str(), id);

                tracks->Release();

                if (newPlaylistId > 0) {
                    this->RespondWithOptions(connection, request, {
                        { key::playlist_id, newPlaylistId }
                    });
                    return;
                }

                this->RespondWithFailure(connection, request);
                return;
            }
        }
    }

    this->RespondWithInvalidRequest(
        connection, request[message::name], request[message::id]);
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

void WebSocketServer::RespondWithAppendToPlaylist(connection_hdl connection, json& request) {
    /* TODO: a lot of copy/paste between this method and RespondWithSavePlaylist */

    auto& options = request[message::options];
    int offset = options.value(key::offset, -1);
    int64_t id = options.value<int64_t>(key::playlist_id, 0);

    if (id) {
        /* by external id */
        if (options.find(key::external_ids) != options.end()) {
            json& externalIds = options[key::external_ids];

            if (externalIds.is_array()) {
                auto externalIdArray = jsonToStringArray(externalIds);

                bool result = this->context.dataProvider
                    ->AppendToPlaylistWithExternalIds(
                        id,
                        (const char**) externalIdArray.get(),
                        externalIds.size(),
                        offset);

                result
                    ? this->RespondWithSuccess(connection, request)
                    : this->RespondWithFailure(connection, request);

                return;
            }
        }
        /* by subquery (query_tracks or query_tracks_by_category) */
        else if (options.find(key::subquery) != options.end()) {
            auto& subquery = options[key::subquery];
            std::string type = subquery.value(key::type, "");

            if (subquery.find(message::options) != subquery.end()) {
                ITrackList* tracks = nullptr;
                int queryLimit, queryOffset;
                if (type == request::query_tracks) {
                    tracks = this->QueryTracks(subquery, queryLimit, queryOffset);
                }
                else if (type == request::query_tracks_by_category) {
                    tracks = this->QueryTracksByCategory(subquery, queryLimit, queryOffset);
                }

                if (tracks) {
                    bool result = this->context.dataProvider
                        ->AppendToPlaylistWithTrackList(id, tracks, offset);

                    tracks->Release();

                    result
                        ? this->RespondWithSuccess(connection, request)
                        : this->RespondWithFailure(connection, request);

                    return;
                }
            }
        }
    }
    /* no id list or external id list */
    this->RespondWithInvalidRequest(
        connection, request[message::name], request[message::id]);
}

void WebSocketServer::RespondWithRunIndexer(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    auto type = options.value(key::type, value::reindex);
    if (type == value::rebuild) {
        context.environment->RebuildMetadata();
    }
    else {
        context.environment->ReindexMetadata();
    }
    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithListOutputDrivers(connection_hdl connection, json& request) {
    json outputs = json::array();

    std::string selectedDriverName, selectedDeviceId;

    size_t count = context.environment->GetOutputCount();
    for (size_t i = 0; i < count; i++) {
        auto output = context.environment->GetDefaultOutput();

        if (output) {
            selectedDriverName = output->Name();
            auto device = output->GetDefaultDevice();
            if (device) {
                selectedDeviceId = device->Id();
            }
            output->Release();
        }

        output = context.environment->GetOutputAtIndex(i);
        json devices = json::array();

        devices.push_back({
            { key::device_name, "default" },
            { key::device_id, "" }
        });

        auto deviceList = output->GetDeviceList();
        if (deviceList) {
            for (size_t j = 0; j < deviceList->Count(); j++) {
                auto device = deviceList->At(j);
                devices.push_back({
                    { key::device_name, device->Name() },
                    { key::device_id, device->Id() }
                });
            }
            deviceList->Release();
        }

        outputs.push_back({
            { key::driver_name, output->Name() },
            { key::devices, devices }
        });

        output->Release();
    }

    this->RespondWithOptions(connection, request, {
        { key::all, outputs },
        { key::selected, {
            { key::driver_name, selectedDriverName },
            { key::device_id, selectedDeviceId }
        }
    }});
}

void WebSocketServer::RespondWithSetDefaultOutputDriver(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    std::string driver = options.value(key::driver_name, "");
    if (driver.size()) {
        auto output = context.environment->GetOutputWithName(driver.c_str());
        if (output) {
            std::string device = options.value(key::device_id, "");
            output->SetDefaultDevice(device.c_str());
            context.environment->SetDefaultOutput(output);
            output->Release();
            this->RespondWithSuccess(connection, request);
            return;
        }
    }
    this->RespondWithFailure(connection, request);
}

void WebSocketServer::RespondWithGetGainSettings(connection_hdl connection, json& request) {
    auto replayGainMode = context.environment->GetReplayGainMode();
    float preampGain = context.environment->GetPreampGain();

    this->RespondWithOptions(connection, request, {
        { key::replaygain_mode, REPLAYGAIN_MODE_TO_STRING.left.find(replayGainMode)->second },
        { key::preamp_gain, preampGain }
    });
}

void WebSocketServer::RespondWithSetGainSettings(connection_hdl connection, json& request) {
    bool reload = false;

    auto& options = request[message::options];

    float currentGain = context.environment->GetPreampGain();
    auto currentMode = context.environment->GetReplayGainMode();
    auto currentModeString = REPLAYGAIN_MODE_TO_STRING.left.find(currentMode)->second;

    ReplayGainMode newMode = REPLAYGAIN_MODE_TO_STRING.right.find(
        options.value(key::replaygain_mode, currentModeString))->second;

    float newGain = options.value(key::preamp_gain, currentGain);

    if (newMode != currentMode) {
        context.environment->SetReplayGainMode(newMode);
        reload = true;
    }

    if (newGain != currentGain) {
        context.environment->SetPreampGain(newGain);
        reload = true;
    }

    if (reload) {
        context.environment->ReloadPlaybackOutput();
    }

    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithGetTransportType(connection_hdl connection, json& request) {
    auto type = context.environment->GetTransportType();
    this->RespondWithOptions(connection, request, {
        { key::type, TRANSPORT_TYPE_TO_STRING.left.find(type)->second }
    });
}

void WebSocketServer::RespondWithSetTransportType(connection_hdl connection, json& request) {
    auto& options = request[message::options];

    std::string currentType = TRANSPORT_TYPE_TO_STRING.left
        .find(context.environment->GetTransportType())->second;

    auto newType = options.value(key::type, currentType);

    if (currentType != newType) {
        auto enumType = TRANSPORT_TYPE_TO_STRING.right.find(newType)->second;
        context.environment->SetTransportType(enumType);
    }

    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithSnapshotPlayQueue(connection_hdl connection, json& request) {
    auto deviceId = request[message::device_id];
    this->snapshots.Remove(deviceId);
    this->snapshots.Put(deviceId, context.playback->Clone());
    this->RespondWithSuccess(connection, request);
}

void WebSocketServer::RespondWithRemoveTracksFromPlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    auto end = options.end();
    auto externalIdsIt = options.find(key::external_ids);
    auto sortOrdersIt = options.find(key::sort_orders);
    int64_t id = options.value<int64_t>(key::playlist_id, 0);
    size_t updated = 0;

    bool valid =
        id != 0 &&
        externalIdsIt != end &&
        sortOrdersIt != end &&
        (*externalIdsIt).size() == (*sortOrdersIt).size();

    if (valid) {
        auto count = (*externalIdsIt).size();
        if (count > 0) {
            auto ids = jsonToStringArray(*externalIdsIt);
            auto orders = jsonToIntArray<int>(*sortOrdersIt);

            updated = this->context.dataProvider
                ->RemoveTracksFromPlaylist(
                    id,
                    (const char**)ids.get(),
                    orders.get(),
                    count);
        }
    }

    this->RespondWithOptions(connection, request, {
        { key::count, updated }
    });
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

    /* note that sometimes multiple independent components will request an
    overview broadcast, so we always remember the last one, and won't
    re-broadcast if status hasn't changed */
    std::string newPlaybackOverview = options.dump();
    if (newPlaybackOverview != lastPlaybackOverview) {
        this->Broadcast(broadcast::playback_overview_changed, options);
        this->lastPlaybackOverview = newPlaybackOverview;
    }
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

json WebSocketServer::WebSocketServer::ReadTrackMetadata(ITrack* track) {
    return {
        { key::id, track ? track->GetId() : -1LL },
        { key::external_id, GetMetadataString(track, key::external_id) },
        { key::title, GetMetadataString(track, key::title) },
        { key::track_num, GetMetadataInt32(track, key::track_num.c_str(), 0) },
        { key::album, GetMetadataString(track, key::album) },
        { key::album_id, GetMetadataInt64(track, key::album_id.c_str()) },
        { key::album_artist, GetMetadataString(track, key::album_artist) },
        { key::album_artist_id, GetMetadataInt64(track, key::album_artist_id.c_str()) },
        { key::artist, GetMetadataString(track, key::artist) },
        { key::artist_id, GetMetadataInt64(track, key::visual_artist_id.c_str()) },
        { key::genre, GetMetadataString(track, key::genre) },
        { key::genre_id, GetMetadataInt64(track, key::visual_genre_id.c_str()) },
        { key::thumbnail_id, GetMetadataInt64(track, key::thumbnail_id.c_str()) },
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

    ITrack* track = context.playback->GetPlayingTrack();
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
