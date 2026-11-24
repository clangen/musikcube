//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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
#include "ServerControls.h"

#include <musikcore/sdk/version.h>
#include <musikcore/sdk/String.h>
#include <array>
#include <limits>
#include <charconv>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using namespace nlohmann;
using namespace musik::core::sdk;

static int nextId = 0;
static const char* TAG = "WebSocketServer";

namespace {
    mc_string_view view(std::string_view text) { return { text.data(), text.size() }; }

    std::string string(mc_string_view value) {
        return std::string(value.data ? value.data : "", value.length);
    }

    struct JsonRows {
        json rows = json::array();
        bool failed { false };
    };

    struct ReleaseTracks {
        void operator()(mc_track_result* tracks) const { mc_track_result_destroy(&tracks); }
    };

    struct ReleasePage {
        void operator()(mc_track_page* page) const { mc_track_page_destroy(&page); }
    };

    struct ReleaseQueue {
        void operator()(mc_play_queue* queue) const { mc_play_queue_destroy(&queue); }
    };

    using Tracks = std::unique_ptr<mc_track_result, ReleaseTracks>;
    using Page = std::unique_ptr<mc_track_page, ReleasePage>;
    using Queue = std::unique_ptr<mc_play_queue, ReleaseQueue>;

    enum TrackField : size_t {
        ExternalId, Title, TrackNumber, Album, AlbumId, AlbumArtist, AlbumArtistId,
        Artist, ArtistId, Genre, GenreId, ThumbnailId
    };

    const mc_string_view TrackFields[] {
        view(MC_TRACK_FIELD_EXTERNAL_ID), view(MC_TRACK_FIELD_TITLE), view(MC_TRACK_FIELD_TRACK),
        view(MC_TRACK_FIELD_ALBUM), view(MC_TRACK_FIELD_ALBUM_ID), view(MC_TRACK_FIELD_ALBUM_ARTIST),
        view(MC_TRACK_FIELD_ALBUM_ARTIST_ID), view(MC_TRACK_FIELD_ARTIST), view(MC_TRACK_FIELD_VISUAL_ARTIST_ID),
        view(MC_TRACK_FIELD_GENRE), view(MC_TRACK_FIELD_VISUAL_GENRE_ID), view(MC_TRACK_FIELD_THUMBNAIL_ID),
    };

    /* project rows after the owner thread has released the queue snapshot. */
    Page readTracks(const mc_library_host_api& library, const mc_track_result* tracks, size_t offset, size_t limit) {
        auto range = mc_track_range MC_TRACK_RANGE_DEFAULT;
        range.offset = offset;
        range.limit = limit;
        range.fields = TrackFields;
        range.field_count = sizeof(TrackFields) / sizeof(TrackFields[0]);
        mc_track_page* page = nullptr;
        if (library.read_tracks(library.context, tracks, &range, &page) != MC_OK) {
            return {};
        }
        return Page(page);
    }

    std::string fieldText(const mc_track_page* page, size_t row, size_t field) {
        const auto value = mc_track_page_get_string(page, row, field);
        return std::string(value.data ? value.data : "", value.length);
    }

    int64_t fieldInteger(const mc_track_page* page, size_t row, size_t field) {
        const auto value = mc_track_page_get_string(page, row, field);
        int64_t result = 0;
        if (value.data) {
            std::from_chars(value.data, value.data + value.length, result);
        }
        return result;
    }

    json trackMetadata(const mc_track_page* page, size_t row, bool matchFields = false) {
        std::array<size_t, sizeof(TrackFields) / sizeof(TrackFields[0])> indexes {};
        for (size_t i = 0; i < indexes.size(); ++i) {
            indexes[i] = matchFields ? SIZE_MAX : i;
            if (matchFields) {
                for (size_t j = 0; j < mc_track_page_get_field_count(page); ++j) {
                    const auto name = mc_track_page_get_field_name(page, j);
                    if (name.length == TrackFields[i].length &&
                        std::string_view(name.data, name.length) ==
                            std::string_view(TrackFields[i].data, TrackFields[i].length))
                    {
                        indexes[i] = j;
                        break;
                    }
                }
            }
        }
        return {
            { key::id, mc_track_page_get_id(page, row) },
            { key::external_id, fieldText(page, row, indexes[ExternalId]) },
            { key::title, fieldText(page, row, indexes[Title]) },
            { key::track_num, fieldInteger(page, row, indexes[TrackNumber]) },
            { key::album, fieldText(page, row, indexes[Album]) },
            { key::album_id, fieldInteger(page, row, indexes[AlbumId]) },
            { key::album_artist, fieldText(page, row, indexes[AlbumArtist]) },
            { key::album_artist_id, fieldInteger(page, row, indexes[AlbumArtistId]) },
            { key::artist, fieldText(page, row, indexes[Artist]) },
            { key::artist_id, fieldInteger(page, row, indexes[ArtistId]) },
            { key::genre, fieldText(page, row, indexes[Genre]) },
            { key::genre_id, fieldInteger(page, row, indexes[GenreId]) },
            { key::thumbnail_id, fieldInteger(page, row, indexes[ThumbnailId]) },
        };
    }
}

/* UTILITY METHODS */

static std::string nextMessageId() {
    return str::Format("musikcube-server-%d", ++nextId);
}

static json getEnvironment(Context& context) {
    return {
        { serverprefs::AudioEnabled, context.settings.Boolean(serverprefs::AudioEnabled.c_str()) },
        { serverprefs::AudioPort, context.settings.Integer(serverprefs::AudioPort.c_str()) },
        { key::sdk_version, ClientSdkVersion },
        { key::app_version, MUSIKCUBE_VERSION },
        { key::contextual_facets, true },
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
        auto created = std::make_shared<server>();
        created->init_asio();
        {
            std::lock_guard lock(this->serverMutex);
            this->wss = created;
        }
        if (this->context.stopping.load()) {
            created->stop();
        }

        if (context.settings.Boolean("debug")) {
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

        const int port = context.settings.Integer(
            serverprefs::MetadataPort.c_str(), serverprefs::DefaultMetadataPort);

        const bool ipv6 = context.settings.Boolean(
            serverprefs::UseIpv6.c_str(), defaults::use_ipv6);

        wss->set_reuse_addr(true);
        wss->set_message_handler(std::bind(&WebSocketServer::OnMessage, this, wss.get(), ::_1, ::_2));
        wss->set_open_handler(std::bind(&WebSocketServer::OnOpen, this, ::_1));
        wss->set_close_handler(std::bind(&WebSocketServer::OnClose, this, ::_1));
        wss->listen(ipv6 ? tcp::v6() : tcp::v4(), port);
        wss->start_accept();

        wss->run();
    }
    catch (websocketpp::exception const & e) {
        this->context.Log(MC_LOG_ERROR, TAG, str::Format("[ThreadProc] websocketpp::exception: %s", e.what()).c_str());
    }
    catch (std::exception& e) {
        this->context.Log(MC_LOG_ERROR, TAG, str::Format("[ThreadProc] sttd::exception: %s", e.what()).c_str());
    }
    catch (...) {
        this->context.Log(MC_LOG_ERROR, TAG, "[ThreadProc] unknown/unexpected exception");

    }

    {
        std::lock_guard lock(this->serverMutex);
        this->wss.reset();
    }
    this->running = false;
    this->snapshots.Reset();

    this->exitCondition.notify_all();
}

bool WebSocketServer::Stop() {
    if (this->thread) {
        std::shared_ptr<server> current;
        {
            std::lock_guard lock(this->serverMutex);
            current = this->wss;
        }
        if (current) {
            current->stop();
        }

        this->thread->join();
        this->thread.reset();
    }

    this->running = false;
    this->exitCondition.notify_all();

    return true;
}

/* run the engine operation on its owner without blocking the network thread.
the host settles accepted work before destroying this server. */
void WebSocketServer::DispatchPlayback(std::function<mc_result(mc_playback*)> operation,
    std::function<void(mc_result)> completion, connection_hdl connection)
{
    struct Work {
        WebSocketServer* server;
        connection_hdl connection;
        std::function<mc_result(mc_playback*)> operation;
        std::function<void(mc_result)> completion;
    };
    auto work = std::unique_ptr<Work>(new Work {
        .server = this,
        .connection = connection,
        .operation = std::move(operation),
        .completion = std::move(completion),
    });
    auto* pending = work.release();
    const auto status = this->context.playbackRemote->with_playback_async(
        this->context.playbackRemote->context, [](void* data, mc_result result, mc_playback* playback) {
            std::unique_ptr<Work> work(static_cast<Work*>(data));
            if (result == MC_OK) {
                try {
                    result = work->operation(playback);
                }
                catch (...) {
                    result = MC_ERROR_INTERNAL;
                }
            }
            mc_playback_destroy(&playback);
            if (!work->server->context.stopping.load()) {
                try {
                    auto completion = std::move(work->completion);
                    work->server->PostToIo([completion = std::move(completion), result] { completion(result); },
                        work->connection);
                }
                catch (...) {
                    work->server->CloseConnection(work->connection);
                }
            }
        }, pending);
    if (status != MC_OK) {
        work.reset(pending);
        auto completion = std::move(work->completion);
        try {
            this->PostToIo([completion = std::move(completion), status] { completion(status); }, connection);
        }
        catch (...) {
            this->CloseConnection(connection);
        }
    }
}

void WebSocketServer::CloseConnection(connection_hdl connection) noexcept {
    if (connection.expired()) {
        return;
    }
    try {
        std::shared_ptr<server> current;
        {
            std::lock_guard lock(this->serverMutex);
            current = this->wss;
        }
        if (current) {
            websocketpp::lib::error_code error;
            current->close(connection, websocketpp::close::status::internal_endpoint_error,
                "server response unavailable", error);
        }
    }
    catch (...) {
        this->context.Log(MC_LOG_ERROR, TAG, "could not close client after response failure");
    }
}

void WebSocketServer::PostToIo(std::function<void()> operation, connection_hdl connection) noexcept {
    try {
        std::shared_ptr<server> current;
        {
            std::lock_guard lock(this->serverMutex);
            current = this->wss;
        }
        if (current && !this->context.stopping.load()) {
            websocketpp::lib::asio::post(current->get_io_context(), [this, operation = std::move(operation), connection] {
                if (!this->context.stopping.load()) {
                    try {
                        operation();
                    }
                    catch (...) {
                        this->context.Log(MC_LOG_ERROR, TAG, "network response failed");
                        this->CloseConnection(connection);
                    }
                }
            });
            return;
        }
    }
    catch (...) {
        this->context.Log(MC_LOG_ERROR, TAG, "could not queue a network response");
    }
    if (!this->context.stopping.load()) {
        this->CloseConnection(connection);
    }
}

void WebSocketServer::DispatchCommand(connection_hdl connection, json request,
    std::function<mc_result(mc_playback*)> operation)
{
    this->DispatchPlayback(std::move(operation), [this, connection, request = std::move(request)](mc_result status) mutable {
        const auto lock = this->connectionLock.Read();
        const auto found = this->connections.find(connection);
        if (found == this->connections.end() || !found->second) {
            return;
        }
        if (status == MC_OK) {
            this->RespondWithSuccess(connection, request);
        }
        else {
            this->RespondWithFailure(connection, request);
        }
    }, connection);
}

/* retains the request until its asynchronous track read can be sent. */
struct WebSocketServer::QueueResponse {
    WebSocketServer* owner { nullptr };
    connection_hdl connection;
    json request;
    Page page;
    size_t offset { 0 };
    int limit { 0 };
    bool idsOnly { false };
    mc_result status { MC_ERROR_INTERNAL };
};

/* a remote queue may point back here; never wait on the websocket thread for
its page. the owner callback only posts the owned result back to socket IO. */
void MC_CALL WebSocketServer::QueueResponseCompleted(mc_result status, mc_track_page* page,
    const mc_error*, void* data) noexcept
{
    std::unique_ptr<QueueResponse> reply(static_cast<QueueResponse*>(data));
    reply->page.reset(page);
    reply->status = status;
    auto& owner = *reply->owner;
    if (owner.context.stopping.load()) {
        return;
    }
    std::shared_ptr<server> current;
    {
        std::lock_guard lock(owner.serverMutex);
        current = owner.wss;
    }
    if (!current) {
        return;
    }
    const auto connection = reply->connection;
    try {
        websocketpp::lib::asio::post(current->get_io_context(), [reply = std::move(reply)]() mutable {
            auto& owner = *reply->owner;
            const auto target = reply->connection;
            if (owner.context.stopping.load()) {
                return;
            }
            try {
                const auto lock = owner.connectionLock.Read();
                const auto connection = owner.connections.find(reply->connection);
                if (connection == owner.connections.end() || !connection->second) {
                    return;
                }
                if (reply->status != MC_OK || !reply->page) {
                    owner.RespondWithFailure(reply->connection, reply->request);
                    return;
                }
                json rows = json::array();
                const auto count = mc_track_page_get_count(reply->page.get());
                for (size_t i = 0; i < count; ++i) {
                    rows.push_back(reply->idsOnly
                        ? json(fieldText(reply->page.get(), i, ExternalId)) : trackMetadata(reply->page.get(), i));
                }
                owner.RespondWithOptions(reply->connection, reply->request, {
                    { key::data, rows }, { key::count, rows.size() },
                    { key::limit, reply->limit }, { key::offset, reply->offset },
                });
            }
            catch (...) {
                /* projection can fail before a response has been built. */
                if (reply) {
                    try {
                        owner.RespondWithFailure(target, reply->request);
                        return;
                    }
                    catch (...) {
                    }
                }
                /* if even the failure cannot be sent, wake the client by closing. */
                try {
                    websocketpp::lib::error_code error;
                    owner.wss->close(target, websocketpp::close::status::internal_endpoint_error,
                        "queue response failed", error);
                }
                catch (...) {
                    owner.context.Log(MC_LOG_WARNING, TAG, "could not close client after queue response failed");
                }
            }
        });
    }
    catch (...) {
        if (!owner.context.stopping.load()) {
            try {
                websocketpp::lib::error_code error;
                current->close(connection, websocketpp::close::status::internal_endpoint_error,
                    "queue response unavailable", error);
                if (error) {
                    owner.context.Log(MC_LOG_WARNING, TAG, "could not close client after queue response failed");
                }
            }
            catch (...) {
                owner.context.Log(MC_LOG_WARNING, TAG, "could not close client after queue response failed");
            }
        }
    }
}

/* post to the network worker so the playback owner never waits for a client. */
void WebSocketServer::PostPlaybackChanged(bool queue) noexcept {
    try {
        this->PostToIo([this, queue] {
            if (queue) {
                this->BroadcastPlayQueueChanged();
                return;
            }
            {
                const auto lock = this->connectionLock.Read();
                if (this->connections.empty()) {
                    return;
                }
            }
            auto options = std::make_shared<json>();
            this->DispatchPlayback([this, options](mc_playback* playback) {
                this->BuildPlaybackOverview(*options, playback);
                return MC_OK;
            }, [this, options](mc_result result) {
                if (result != MC_OK) {
                    return;
                }
                const auto serialized = options->dump();
                if (serialized != this->lastPlaybackOverview) {
                    this->Broadcast(broadcast::playback_overview_changed, *options);
                    this->lastPlaybackOverview = serialized;
                }
            });
        });
    }
    catch (...) {
        this->context.Log(MC_LOG_ERROR, TAG, "could not queue playback notification");
    }
}

void WebSocketServer::HandleAuthentication(connection_hdl connection, json& request) {
    std::string name = request[message::name];

    if (name == request::authenticate) {
        std::string sent = request[message::options][key::password];

        std::string actual = context.settings.Text(serverprefs::Password, defaults::password);

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
    else if (context.playbackRemote) {
        if (request.find(message::options) == request.end()) {
            request[message::options] = {};
        }

        const json& options = request[message::options];

        if (name == request::ping) {
            this->RespondWithSuccess(connection, request);
            return;
        }
        if (name == request::send_raw_query) {
            this->RespondWithLibraryQuery(connection, request);
            return;
        }
        if (name == request::pause_or_resume || name == request::stop ||
            name == request::previous || name == request::next || name == request::toggle_shuffle ||
            name == request::toggle_repeat || name == request::toggle_mute)
        {
            mc_playback_command command = MC_PLAYBACK_STOP;
            if (name == request::pause_or_resume) {
                command = MC_PLAYBACK_TOGGLE_PAUSE;
            }
            else if (name == request::previous) {
                command = MC_PLAYBACK_PREVIOUS;
            }
            else if (name == request::next) {
                command = MC_PLAYBACK_NEXT;
            }
            else if (name == request::toggle_shuffle) {
                command = MC_PLAYBACK_TOGGLE_SHUFFLE;
            }
            else if (name == request::toggle_repeat) {
                command = MC_PLAYBACK_TOGGLE_REPEAT;
            }
            else if (name == request::toggle_mute) {
                command = MC_PLAYBACK_TOGGLE_MUTE;
            }
            this->DispatchCommand(connection, request, [command](mc_playback* playback) {
                return mc_playback_control(playback, command, 0);
            });
            return;
        }
        else if (name == request::play_at_index) {
            if (!options.contains(key::index)) {
                this->RespondWithFailure(connection, request);
                return;
            }
            const auto index = options[key::index].get<size_t>();
            this->DispatchCommand(connection, request, [index](mc_playback* playback) {
                mc_play_queue* queue = nullptr;
                const auto status = mc_playback_get_queue(playback, &queue);
                const auto result = status == MC_OK ? mc_playback_play_queue_row(playback, queue, index) : status;
                mc_play_queue_destroy(&queue);
                return result;
            });
            return;
        }
        else if (name == request::set_volume) {
            this->RespondWithSetVolume(connection, request);
            return;
        }
        else if (name == request::seek_to || name == request::seek_relative) {
            const bool relative = name == request::seek_relative;
            if ((!relative && !options.contains(key::position)) ||
                (relative && options.value(key::delta, 0.0) == 0.0))
            {
                this->RespondWithFailure(connection, request);
                return;
            }
            const auto value = options.value(relative ? key::delta : key::position, 0.0);
            this->DispatchCommand(connection, request, [relative, value](mc_playback* playback) {
                auto position = value;
                if (relative) {
                    auto snapshot = mc_playback_snapshot MC_PLAYBACK_SNAPSHOT_DEFAULT;
                    const auto status = mc_playback_get_snapshot(playback, &snapshot);
                    if (status != MC_OK) {
                        return status;
                    }
                    position = std::max(0.0, snapshot.position + value);
                }
                return mc_playback_control(playback, MC_PLAYBACK_SET_POSITION, position);
            });
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
        else if (name == request::get_equalizer_settings) {
            this->RespondWithGetEqualizerSettings(connection, request);
            return;
        }
        else if (name == request::set_equalizer_settings) {
            this->RespondWithSetEqualizerSettings(connection, request);
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
        this->context.Log(MC_LOG_ERROR, TAG, "broadcast failed (stale connection?)");
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

void WebSocketServer::RespondWithLibraryQuery(connection_hdl connection, json& request) {
    json& options = request[message::options];
    const auto data = options.value(key::raw_query_data, std::string());
    struct Result {
        std::string text;
        bool failed { false };
    } result;
    const auto receive = [](void* context, mc_string_view text) {
        auto& result = *static_cast<Result*>(context);
        try {
            result.text.assign(text.data ? text.data : "", text.length);
        }
        catch (...) {
            result.failed = true;
        }
    };
    const auto& library = *this->context.library;
    const auto status = library.run_query(library.context, view(data), receive, &result);
    if (status == MC_OK && !result.failed && !result.text.empty()) {
        this->RespondWithOptions(connection, request, { { key::raw_query_data, result.text } });
        return;
    }
    this->RespondWithFailure(connection, request);
}

void WebSocketServer::RespondWithSetVolume(connection_hdl connection, json& request) {
    json& options = request[message::options];
    std::string relative = options.value(key::relative, "");
    const double requested = options.value(key::volume, 0.0);
    this->DispatchCommand(connection, request, [relative, requested](mc_playback* playback) {
        auto snapshot = mc_playback_snapshot MC_PLAYBACK_SNAPSHOT_DEFAULT;
        const auto result = mc_playback_get_snapshot(playback, &snapshot);
        if (result != MC_OK) {
            return result;
        }
        double volume = requested;
        if (relative == value::up) {
            volume = snapshot.volume + (round(snapshot.volume * 100.0) >= 10.0 ? 0.05 : 0.01);
        }
        else if (relative == value::down) {
            volume = snapshot.volume - (round(snapshot.volume * 100.0) > 10.0 ? 0.05 : 0.01);
        }
        else if (relative == value::delta) {
            volume = snapshot.volume + requested;
        }
        return mc_playback_control(playback, MC_PLAYBACK_SET_VOLUME, std::clamp(volume, 0.0, 1.0));
    });
}

void WebSocketServer::RespondWithPlaybackOverview(connection_hdl connection, json& request) {
    auto options = std::make_shared<json>();
    this->DispatchPlayback([this, options](mc_playback* playback) {
        this->BuildPlaybackOverview(*options, playback);
        return MC_OK;
    }, [this, connection, request, options](mc_result status) mutable {
        const auto lock = this->connectionLock.Read();
        const auto found = this->connections.find(connection);
        if (found == this->connections.end() || !found->second) {
            return;
        }
        if (status == MC_OK) {
            this->RespondWithOptions(connection, request, *options);
        }
        else {
            this->RespondWithFailure(connection, request);
        }
    }, connection);
}

bool WebSocketServer::RespondWithTracks(
    connection_hdl connection, json& request, mc_track_result* ownedTracks, int limit, int offset)
{
    Tracks tracks(ownedTracks);
    if (!tracks) {
        return false;
    }
    const auto& options = request[message::options];
    const size_t count = mc_track_result_get_count(tracks.get());
    if (options.value(key::count_only, false)) {
        this->RespondWithOptions(connection, request, {
            { key::data, json::array() },
            { key::count, count },
        });
        return true;
    }
    json data = json::array();
    if (count) {
        const auto page = readTracks(*context.library, tracks.get(), 0, count);
        if (!page) {
            return false;
        }
        const bool idsOnly = options.value(key::ids_only, false);
        for (size_t i = 0; i < mc_track_page_get_count(page.get()); ++i) {
            data.push_back(idsOnly ? json(fieldText(page.get(), i, ExternalId)) : trackMetadata(page.get(), i));
        }
    }
    this->RespondWithOptions(connection, request, {
        { key::data, data },
        { key::count, data.size() },
        { key::limit, std::max(0, limit) },
        { key::offset, offset },
    });
    return true;
}

void WebSocketServer::GetLimitAndOffset(json& options, int& limit, int& offset) {
    int optionsLimit = options.value(key::limit, -1);
    int optionsOffset = options.value(key::offset, 0);
    if (optionsLimit != -1 && optionsOffset >= 0) {
        limit = optionsLimit;
        offset = optionsOffset;
    }
}

mc_track_result* WebSocketServer::QueryTracks(json& request, int& limit, int& offset) {
    if (request.find(message::options) == request.end()) {
        return nullptr;
    }
    auto& options = request[message::options];
    const std::string filter = options.value(key::filter, "");
    this->GetLimitAndOffset(options, limit, offset);
    auto search = mc_track_search MC_TRACK_SEARCH_DEFAULT;
    search.text = view(filter);
    mc_track_result* tracks = nullptr;
    const auto& library = *context.library;
    if (library.search_tracks(library.context, &search, limit, offset, &tracks) != MC_OK) {
        return nullptr;
    }
    return tracks;
}

void WebSocketServer::RespondWithQueryTracks(connection_hdl connection, json& request) {
    if (request.find(message::options) != request.end()) {
        int limit = -1, offset = 0;
        mc_track_result* tracks = this->QueryTracks(request, limit, offset);
        if (this->RespondWithTracks(connection, request, tracks, limit, offset)) {
            return;
        }
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithQueryTracksByExternalIds(connection_hdl connection, json& request) {
    const auto& options = request[message::options];
    if (!options.contains(key::external_ids) || !options[key::external_ids].is_array()) {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    const auto ids = options[key::external_ids].get<std::vector<std::string>>();
    std::vector<mc_string_view> views;
    views.reserve(ids.size());
    for (const auto& id : ids) {
        views.push_back(view(id));
    }
    mc_track_result* raw = nullptr;
    const auto& library = *this->context.library;
    if (library.tracks_by_external_ids(library.context, views.data(), views.size(), &raw) != MC_OK) {
        this->RespondWithFailure(connection, request);
        return;
    }
    Tracks tracks(raw);
    json result = json::object();
    const auto count = mc_track_result_get_count(tracks.get());
    if (count) {
        const auto page = readTracks(library, tracks.get(), 0, count);
        if (!page) {
            this->RespondWithFailure(connection, request);
            return;
        }
        for (size_t i = 0; i < mc_track_page_get_count(page.get()); ++i) {
            result[fieldText(page.get(), i, ExternalId)] = trackMetadata(page.get(), i);
        }
    }
    this->RespondWithOptions(connection, request, { { key::data, result } });
}

void WebSocketServer::RespondWithPlayQueueTracks(connection_hdl connection, json& request) {
    const auto type = request[message::options].value(key::type, value::live);
    if (type == value::live) {
        auto queue = std::make_shared<Queue>();
        this->DispatchPlayback([queue](mc_playback* playback) {
            mc_play_queue* raw = nullptr;
            const auto status = mc_playback_get_queue(playback, &raw);
            queue->reset(raw);
            return status;
        }, [this, connection, request, queue](mc_result status) mutable {
            const auto lock = this->connectionLock.Read();
            const auto found = this->connections.find(connection);
            if (found == this->connections.end() || !found->second) {
                return;
            }
            if (status == MC_OK) {
                this->SendQueueTracks(connection, request, queue->get());
            }
            else {
                this->RespondWithFailure(connection, request);
            }
        }, connection);
        return;
    }
    this->SendQueueTracks(connection, request,
        type == value::snapshot ? this->snapshots.Get(request[message::device_id]) : nullptr);
}

void WebSocketServer::SendQueueTracks(connection_hdl connection, json& request, const mc_play_queue* queue) {
    auto& options = request[message::options];
    const bool countOnly = options.value(key::count_only, false);
    const bool idsOnly = options.value(key::ids_only, false);
    int limit = -1;
    int offset = 0;
    if (!countOnly) {
        this->GetLimitAndOffset(options, limit, offset);
        if (options.contains(key::offset)) {
            const auto requestedOffset = options.value(key::offset, 0);
            if (requestedOffset >= 0) {
                offset = requestedOffset;
            }
        }
    }
    size_t count = 0;
    if (queue) {
        mc_play_queue_get_ids(queue, &count);
    }
    if (countOnly) {
        this->RespondWithOptions(connection, request, { { key::data, json::array() }, { key::count, count } });
        return;
    }
    json data = json::array();
    if (queue && static_cast<size_t>(offset) < count) {
        mc_track_result* raw = nullptr;
        if (mc_play_queue_get_tracks(queue, &raw) != MC_OK) {
            this->RespondWithFailure(connection, request);
            return;
        }
        Tracks tracks(raw);
        const auto available = count - static_cast<size_t>(offset);
        const auto requested = limit < 0 ? available : std::min(available, static_cast<size_t>(limit));
        const auto length = std::min(requested, static_cast<size_t>(MC_TRACK_QUERY_MAX_LIMIT));
        if (length) {
            auto reply = std::make_unique<QueueResponse>();
            reply->owner = this;
            reply->connection = connection;
            reply->request = request;
            reply->offset = static_cast<size_t>(offset);
            reply->limit = static_cast<int>(length);
            reply->idsOnly = idsOnly;

            auto range = mc_track_range MC_TRACK_RANGE_DEFAULT;
            range.offset = reply->offset;
            range.limit = length;
            range.fields = TrackFields;
            range.field_count = sizeof(TrackFields) / sizeof(TrackFields[0]);
            auto* pending = reply.release();
            const auto& library = *this->context.library;
            const auto status = library.read_tracks_async(library.context, tracks.get(), &range,
                QueueResponseCompleted, pending);
            if (status != MC_OK) {
                std::unique_ptr<QueueResponse> rejected(pending);
                this->RespondWithFailure(connection, request);
            }
            return;
        }
    }
    this->RespondWithOptions(connection, request, {
        { key::data, data }, { key::count, data.size() },
        { key::limit, std::max(0, limit) }, { key::offset, offset },
    });
}

void WebSocketServer::RespondWithQueryAlbums(connection_hdl connection, json& request) {
    const auto& options = request[message::options];
    const std::string filter = options.value(key::filter, "");
    const std::string category = options.value(key::category, "");
    const int64_t id = options.value<int64_t>(key::category_id, -1);
    const mc_library_category_selection selection { .field = view(category), .id = id };
    JsonRows result;
    const auto collect = [](void* data, const mc_library_album* albums, size_t count) {
        auto& output = *static_cast<JsonRows*>(data);
        try {
            for (size_t i = 0; i < count; ++i) {
                const auto& album = albums[i];
                output.rows.push_back({
                    { key::title, string(album.title) }, { key::id, album.id },
                    { key::thumbnail_id, album.has_thumbnail_id ? album.thumbnail_id : 0 },
                    { key::album_artist_id, album.has_artist_id ? album.artist_id : 0 },
                    { key::album_artist, string(album.artist) },
                });
            }
        }
        catch (...) {
            output.failed = true;
        }
    };
    const auto& library = *context.library;
    const bool hasSelection = !category.empty() && id >= 0;
    const auto status = library.list_albums(library.context, view(filter),
        hasSelection ? &selection : nullptr, hasSelection ? 1 : 0, collect, &result);
    if (status != MC_OK || result.failed) {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    this->RespondWithOptions(connection, request, { { key::category, key::album }, { key::data, result.rows } });
}

void WebSocketServer::RespondWithPlayTracks(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    const auto& library = *context.library;
    mc_track_result* raw = nullptr;
    mc_result status = MC_ERROR_INVALID_ARGUMENT;
    if (options.contains(key::ids) && options[key::ids].is_array()) {
        const auto ids = options[key::ids].get<std::vector<int64_t>>();
        status = library.tracks_by_ids(library.context, ids.data(), ids.size(), &raw);
    }
    else if (options.contains(key::external_ids) && options[key::external_ids].is_array()) {
        const auto ids = options[key::external_ids].get<std::vector<std::string>>();
        std::vector<mc_string_view> views;
        views.reserve(ids.size());
        for (const auto& id : ids) {
            views.push_back(view(id));
        }
        status = library.tracks_by_external_ids(library.context, views.data(), views.size(), &raw);
    }
    if (status != MC_OK || !raw || !mc_track_result_get_count(raw)) {
        mc_track_result_destroy(&raw);
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    this->DispatchPlayTracks(connection, request, raw,
        options.value(key::index, size_t { 0 }), options.value(key::time, 0.0));
}

mc_track_result* WebSocketServer::QueryTracksByCategory(json& request, int& limit, int& offset) {
    if (!request.contains(message::options)) {
        return nullptr;
    }
    const auto& options = request[message::options];
    const std::string category = options.value(key::category, "");
    const int64_t selectedId = options.value<int64_t>(key::id, -1);
    const auto predicates = options.value(key::predicates, json::array());
    const std::string filter = options.value(key::filter, "");
    limit = -1;
    offset = 0;
    auto& mutableOptions = request[message::options];
    this->GetLimitAndOffset(mutableOptions, limit, offset);
    const auto& library = *context.library;
    mc_track_result* tracks = nullptr;
    mc_result status;
    if (!predicates.empty()) {
        std::vector<std::string> fields;
        std::vector<mc_library_category_selection> selections;
        fields.reserve(predicates.size());
        selections.reserve(predicates.size());
        for (const auto& predicate : predicates) {
            fields.push_back(predicate.at("category").get<std::string>());
            selections.push_back({ .field = view(fields.back()), .id = predicate.at("id").get<int64_t>() });
        }
        status = library.tracks_by_categories(library.context, selections.data(), selections.size(),
            view(filter), MC_TRACK_SEARCH_ALBUM, limit, offset, &tracks);
    }
    else {
        status = library.tracks_by_category(library.context, view(category), std::max<int64_t>(0, selectedId),
            view(filter), limit, offset, &tracks);
    }
    return status == MC_OK ? tracks : nullptr;
}

void WebSocketServer::RespondWithQueryTracksByCategory(connection_hdl connection, json& request) {
    int limit, offset;

    mc_track_result* tracks = QueryTracksByCategory(request, limit, offset);

    if (tracks && this->RespondWithTracks(connection, request, tracks, limit, offset)) {
        return;
    }

    this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
}

void WebSocketServer::RespondWithListCategories(connection_hdl connection, json& request) {
    JsonRows result;
    const auto collect = [](mc_result status, const mc_string_view* fields, size_t count,
        const mc_error*, void* data) {
        auto& output = *static_cast<JsonRows*>(data);
        try {
            if (status != MC_OK) {
                output.failed = true;
                return;
            }
            for (size_t i = 0; i < count; ++i) {
                output.rows.push_back(string(fields[i]));
            }
        }
        catch (...) {
            output.failed = true;
        }
    };
    const auto& library = *context.library;
    if (library.list_category_fields(library.context, collect, &result) != MC_OK || result.failed) {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    this->RespondWithOptions(connection, request, { { key::data, result.rows } });
}

void WebSocketServer::RespondWithQueryCategory(connection_hdl connection, json& request) {
    const auto& options = request[message::options];
    const std::string category = options.value(key::category, "");
    const std::string filter = options.value(key::filter, "");
    if (category.empty()) {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    std::vector<std::string> fields;
    std::vector<mc_library_category_selection> selections;
    const auto predicates = options.value(key::predicates, json::array());
    fields.reserve(predicates.size() + 1);
    selections.reserve(predicates.size() + 1);
    for (const auto& predicate : predicates) {
        fields.push_back(predicate.at("category").get<std::string>());
        selections.push_back({ .field = view(fields.back()), .id = predicate.at("id").get<int64_t>() });
    }
    if (selections.empty()) {
        auto predicate = options.value(key::predicate_category, "");
        const auto id = options.value<int64_t>(key::predicate_id, -1);
        if (!predicate.empty() && id >= 0) {
            fields.push_back(std::move(predicate));
            selections.push_back({ .field = view(fields.back()), .id = id });
        }
    }
    JsonRows result;
    const auto collect = [](mc_result status, const mc_category_entry* entries, size_t count,
        const mc_error*, void* data) {
        auto& output = *static_cast<JsonRows*>(data);
        try {
            if (status != MC_OK) {
                output.failed = true;
                return;
            }
            for (size_t i = 0; i < count; ++i) {
                output.rows.push_back({ { key::id, entries[i].id }, { key::value, string(entries[i].value) } });
            }
        }
        catch (...) {
            output.failed = true;
        }
    };
    const auto& library = *context.library;
    const auto status = selections.empty()
        ? library.list_categories(library.context, view(category), view(filter), MC_TRACK_SEARCH_SUBSTRING,
            collect, &result)
        : library.list_filtered_categories(library.context, view(category), selections.data(), selections.size(),
            view(filter), MC_TRACK_SEARCH_SUBSTRING, false, { nullptr, 0 }, collect, &result);
    if (status != MC_OK || result.failed) {
        this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        return;
    }
    this->RespondWithOptions(connection, request, { { key::category, category }, { key::data, result.rows } });
}

/* the result must stay alive until the owner executes the playback request. */
void WebSocketServer::DispatchPlayTracks(connection_hdl connection, json request,
    mc_track_result* raw, size_t index, double time)
{
    auto tracks = std::shared_ptr<mc_track_result>(raw, ReleaseTracks {});
    this->DispatchCommand(connection, std::move(request), [tracks, index, time](mc_playback* playback) {
        const auto status = mc_playback_play_result(playback, tracks.get(), index);
        return status == MC_OK && time > 0.0
            ? mc_playback_control(playback, MC_PLAYBACK_SET_POSITION, time) : status;
    });
}

void WebSocketServer::RespondWithPlayAllTracks(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    const auto index = options.value(key::index, size_t { 0 });
    const double time = options.value(key::time, 0.0);
    int limit = -1;
    int offset = 0;
    auto* tracks = this->QueryTracks(request, limit, offset);
    if (!tracks) {
        this->RespondWithFailure(connection, request);
        return;
    }
    this->DispatchPlayTracks(connection, request, tracks, index, time);
}

void WebSocketServer::RespondWithPlaySnapshotTracks(connection_hdl connection, json& request) {
    const auto* snapshot = this->snapshots.Get(request[message::device_id]);
    mc_track_result* raw = nullptr;
    auto status = snapshot ? mc_play_queue_get_tracks(snapshot, &raw) : MC_ERROR_NOT_FOUND;
    if (status == MC_OK) {
        this->DispatchPlayTracks(connection, request, raw,
            request[message::options].value(key::index, size_t { 0 }),
            request[message::options].value(key::time, 0.0));
    }
    else if (!snapshot) {
        this->DispatchCommand(connection, request, [](mc_playback* playback) {
            const auto result = mc_playback_control(playback, MC_PLAYBACK_STOP, 0);
            return result == MC_OK ? mc_playback_queue_clear(playback) : result;
        });
    }
    else {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithPlayTracksByCategory(connection_hdl connection, json& request) {
    int limit;
    int offset;
    auto* tracks = this->QueryTracksByCategory(request, limit, offset);
    if (!tracks) {
        this->RespondWithFailure(connection, request);
        return;
    }
    this->DispatchPlayTracks(connection, request, tracks,
        request[message::options].value(key::index, size_t { 0 }),
        request[message::options].value(key::time, 0.0));
}

void WebSocketServer::RespondWithEnvironment(connection_hdl connection, json& request) {
    this->RespondWithOptions(connection, request, getEnvironment(context));
}

void WebSocketServer::RespondWithCurrentTime(connection_hdl connection, json& request) {
    auto output = std::make_shared<json>();
    this->DispatchPlayback([output](mc_playback* playback) {
        auto snapshot = mc_playback_snapshot MC_PLAYBACK_SNAPSHOT_DEFAULT;
        const auto status = mc_playback_get_snapshot(playback, &snapshot);
        mc_track_ref* track = nullptr;
        int64_t id = -1;
        if (status == MC_OK && mc_playback_get_current_track(playback, &track) == MC_OK) {
            id = mc_track_ref_get_id(track);
        }
        mc_track_ref_destroy(&track);
        *output = { { key::playing_current_time, snapshot.position }, { key::id, id } };
        return status;
    }, [this, connection, request, output](mc_result status) mutable {
        const auto lock = this->connectionLock.Read();
        const auto found = this->connections.find(connection);
        if (found == this->connections.end() || !found->second) {
            return;
        }
        if (status == MC_OK) {
            this->RespondWithOptions(connection, request, *output);
        }
        else {
            this->RespondWithFailure(connection, request);
        }
    }, connection);
}

mc_track_result* WebSocketServer::ResolvePlaylistTracks(json& options) {
    const auto& library = *context.library;
    if (options.contains(key::external_ids) && options[key::external_ids].is_array()) {
        const auto ids = options[key::external_ids].get<std::vector<std::string>>();
        std::vector<mc_string_view> views;
        views.reserve(ids.size());
        for (const auto& id : ids) {
            views.push_back(view(id));
        }
        mc_track_result* tracks = nullptr;
        return library.tracks_by_external_ids(library.context, views.data(), views.size(), &tracks) == MC_OK
            ? tracks : nullptr;
    }
    if (options.contains(key::subquery) && options[key::subquery].is_object()) {
        auto& subquery = options[key::subquery];
        const auto type = subquery.value(key::type, "");
        int limit = -1;
        int offset = 0;
        if (type == request::query_tracks) {
            return this->QueryTracks(subquery, limit, offset);
        }
        if (type == request::query_tracks_by_category) {
            return this->QueryTracksByCategory(subquery, limit, offset);
        }
    }
    return nullptr;
}

void WebSocketServer::RespondWithSavePlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    const auto id = options.value<int64_t>(key::playlist_id, 0);
    const std::string name = options.value(key::playlist_name, "");
    Tracks tracks(this->ResolvePlaylistTracks(options));
    if (!tracks) {
        this->RespondWithInvalidRequest(connection, request[message::name], request[message::id]);
        return;
    }
    std::vector<int64_t> ids;
    const auto count = mc_track_result_get_count(tracks.get());
    ids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ids.push_back(mc_track_result_get_id(tracks.get(), i));
    }
    int64_t saved = 0;
    const auto& library = *context.library;
    if (library.save_playlist(library.context, id, view(name), ids.data(), ids.size(), &saved) == MC_OK && saved > 0) {
        this->RespondWithOptions(connection, request, { { key::playlist_id, saved } });
    }
    else {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithRenamePlaylist(connection_hdl connection, json& request) {
    const auto& options = request[message::options];
    const auto id = options[key::playlist_id].get<int64_t>();
    const auto name = options[key::playlist_name].get<std::string>();
    const auto& library = *context.library;
    if (library.rename_playlist(library.context, id, view(name)) == MC_OK) {
        this->RespondWithSuccess(connection, request);
    }
    else {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithDeletePlaylist(connection_hdl connection, json& request) {
    const auto id = request[message::options][key::playlist_id].get<int64_t>();
    const auto& library = *context.library;
    if (library.delete_playlist(library.context, id) == MC_OK) {
        this->RespondWithSuccess(connection, request);
    }
    else {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithAppendToPlaylist(connection_hdl connection, json& request) {
    auto& options = request[message::options];
    const auto id = options.value<int64_t>(key::playlist_id, 0);
    const auto offset = options.value(key::offset, -1);
    Tracks tracks(this->ResolvePlaylistTracks(options));
    if (id <= 0 || !tracks) {
        this->RespondWithInvalidRequest(connection, request[message::name], request[message::id]);
        return;
    }
    std::vector<int64_t> ids;
    const auto count = mc_track_result_get_count(tracks.get());
    ids.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ids.push_back(mc_track_result_get_id(tracks.get(), i));
    }
    const auto& library = *context.library;
    if (library.append_playlist(library.context, id, ids.data(), ids.size(), offset) == MC_OK) {
        this->RespondWithSuccess(connection, request);
    }
    else {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithControl(connection_hdl connection, json& request,
    std::string_view operation, bool includeOptions)
{
    auto output = std::make_shared<json>(json::object());
    const auto options = request[message::options];
    this->DispatchPlayback([output, options, operation = std::string(operation)](mc_playback* playback) {
        return ExecuteServerControl(playback, operation, options, *output) ? MC_OK : MC_ERROR_INVALID_ARGUMENT;
    }, [this, connection, request, output, includeOptions](mc_result status) mutable {
        const auto lock = this->connectionLock.Read();
        const auto found = this->connections.find(connection);
        if (found == this->connections.end() || !found->second) {
            return;
        }
        if (status != MC_OK) {
            this->RespondWithFailure(connection, request);
        }
        else if (includeOptions) {
            this->RespondWithOptions(connection, request, *output);
        }
        else {
            this->RespondWithSuccess(connection, request);
        }
    }, connection);
}

void WebSocketServer::RespondWithRunIndexer(connection_hdl connection, json& request) {
    const auto status = ScheduleServerIndexer(this->context, request[message::options],
        [this, connection, request](mc_result result) mutable {
            try {
                this->PostToIo([this, connection, request = std::move(request), result]() mutable {
                    const auto lock = this->connectionLock.Read();
                    const auto found = this->connections.find(connection);
                    if (found == this->connections.end() || !found->second) {
                        return;
                    }
                    if (result == MC_OK) {
                        this->RespondWithSuccess(connection, request);
                    }
                    else {
                        this->RespondWithFailure(connection, request);
                    }
                }, connection);
            }
            catch (...) {
                this->CloseConnection(connection);
            }
        });
    if (status != MC_OK) {
        this->RespondWithFailure(connection, request);
    }
}

void WebSocketServer::RespondWithListOutputDrivers(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::list_output_drivers, true);
}

void WebSocketServer::RespondWithSetDefaultOutputDriver(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::set_default_output_driver, false);
}

void WebSocketServer::RespondWithGetGainSettings(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::get_gain_settings, true);
}

void WebSocketServer::RespondWithSetGainSettings(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::set_gain_settings, false);
}

void WebSocketServer::RespondWithGetEqualizerSettings(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::get_equalizer_settings, true);
}

void WebSocketServer::RespondWithSetEqualizerSettings(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::set_equalizer_settings, false);
}

void WebSocketServer::RespondWithGetTransportType(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::get_transport_type, true);
}

void WebSocketServer::RespondWithSetTransportType(connection_hdl connection, json& request) {
    this->RespondWithControl(connection, request, request::set_transport_type, false);
}

void WebSocketServer::RespondWithSnapshotPlayQueue(connection_hdl connection, json& request) {
    auto queue = std::make_shared<Queue>();
    this->DispatchPlayback([queue](mc_playback* playback) {
        mc_play_queue* raw = nullptr;
        const auto status = mc_playback_get_queue(playback, &raw);
        queue->reset(raw);
        return status;
    }, [this, connection, request, queue](mc_result status) mutable {
        const auto lock = this->connectionLock.Read();
        const auto found = this->connections.find(connection);
        if (found == this->connections.end() || !found->second) {
            return;
        }
        if (status == MC_OK) {
            const auto device = request[message::device_id].get<std::string>();
            auto* raw = queue->release();
            this->snapshots.Put(device, &raw);
            this->RespondWithSuccess(connection, request);
        }
        else {
            this->RespondWithFailure(connection, request);
        }
    }, connection);
}

void WebSocketServer::RespondWithRemoveTracksFromPlaylist(connection_hdl connection, json& request) {
    const auto& options = request[message::options];
    const auto id = options.value<int64_t>(key::playlist_id, 0);
    size_t removed = 0;
    if (id > 0 && options.contains(key::external_ids) && options.contains(key::sort_orders) &&
        options[key::external_ids].is_array() && options[key::sort_orders].is_array() &&
        options[key::external_ids].size() == options[key::sort_orders].size())
    {
        const auto ids = options[key::external_ids].get<std::vector<std::string>>();
        const auto orders = options[key::sort_orders].get<std::vector<int32_t>>();
        std::vector<mc_library_playlist_entry_key> entries;
        entries.reserve(ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            entries.push_back({ .external_id = view(ids[i]), .position = orders[i] });
        }
        const auto& library = *context.library;
        if (library.remove_playlist_entries(library.context, id, entries.data(), entries.size(), &removed) != MC_OK) {
            this->RespondWithFailure(connection, request);
            return;
        }
    }
    this->RespondWithOptions(connection, request, { { key::count, removed } });
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

void WebSocketServer::BuildPlaybackOverview(json& options, mc_playback* playback) {
    auto snapshot = mc_playback_snapshot MC_PLAYBACK_SNAPSHOT_DEFAULT;
    mc_play_queue* rawQueue = nullptr;
    mc_track_page* rawCurrent = nullptr;
    const auto status = mc_playback_get_snapshot(playback, &snapshot);
    if (status == MC_OK && mc_playback_get_queue(playback, &rawQueue) == MC_OK) {
        mc_playback_get_current_track_metadata(playback, &rawCurrent);
    }
    auto releaseQueue = [](mc_play_queue* queue) { mc_play_queue_destroy(&queue); };
    std::unique_ptr<mc_play_queue, decltype(releaseQueue)> queue(rawQueue, releaseQueue);
    Page current(rawCurrent);
    if (status != MC_OK || !queue) {
        return;
    }
    const char* state = "stopped";
    switch (snapshot.state) {
        case MC_PLAYBACK_PLAYING:
            state = "playing";
            break;
        case MC_PLAYBACK_PAUSED:
            state = "paused";
            break;
        case MC_PLAYBACK_PREPARED:
            state = "prepared";
            break;
        default:
            break;
    }
    const char* repeat = "none";
    switch (snapshot.repeat) {
        case MC_PLAYBACK_REPEAT_TRACK:
            repeat = "track";
            break;
        case MC_PLAYBACK_REPEAT_LIST:
            repeat = "list";
            break;
        default:
            break;
    }
    size_t count = 0;
    mc_play_queue_get_ids(queue.get(), &count);
    const auto row = mc_play_queue_get_current_row(queue.get());
    options[key::state] = state;
    options[key::repeat_mode] = repeat;
    options[key::volume] = snapshot.volume;
    options[key::shuffled] = snapshot.shuffled;
    options[key::muted] = snapshot.muted;
    options[key::play_queue_count] = count;
    options[key::play_queue_position] = row < count ? static_cast<int64_t>(row) : -1;
    options[key::playing_duration] = snapshot.duration;
    options[key::playing_current_time] = snapshot.position;
    if (row < count && current && mc_track_page_get_count(current.get())) {
        /* playback already owns this metadata; querying a remote origin here
        could wait on this same WebSocket worker. */
        options[key::playing_track] = trackMetadata(current.get(), 0, true);
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
    /* shutdown will settle accepted playback commands before joining this worker. */
    if (this->context.stopping.load()) {
        return;
    }
    auto rl = connectionLock.Read();

    try {
        json data = json::parse(msg->get_payload());
        std::string type = data[message::type];
        if (type == type::request) {
            this->HandleRequest(hdl, data);
        }
    }
    catch (std::exception& e) {
        this->context.Log(MC_LOG_ERROR, TAG, str::Format("OnMessage failed: %s", e.what()).c_str());
        this->RespondWithInvalidRequest(hdl, value::invalid, value::invalid);
    }
    catch (...) {
        this->context.Log(MC_LOG_ERROR, TAG, str::Format("message parse failed: %s", msg->get_payload().c_str()).c_str());
        this->RespondWithInvalidRequest(hdl, value::invalid, value::invalid);
    }
}
