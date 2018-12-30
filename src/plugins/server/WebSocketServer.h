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

#include "Context.h"
#include "Snapshots.h"

#include <core/sdk/constants.h>
#include <core/sdk/ITrack.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/server.hpp>

#include <mutex>
#include <condition_variable>

#include <json.hpp>

class WebSocketServer {
    public:
        WebSocketServer(Context& context);
        ~WebSocketServer();

        bool Start();
        bool Stop();
        void Wait();

        void OnTrackChanged(musik::core::sdk::ITrack* track);
        void OnPlaybackStateChanged(musik::core::sdk::PlaybackState state);
        void OnPlaybackTimeChanged(double time);
        void OnVolumeChanged(double volume);
        void OnModeChanged(musik::core::sdk::RepeatMode repeatMode, bool shuffled);
        void OnPlayQueueChanged();

    private:
        /* our special server config that supports gzip */
        struct asio_with_deflate : public websocketpp::config::asio {
            typedef asio_with_deflate type;
            typedef asio base;

            typedef base::concurrency_type concurrency_type;

            typedef base::request_type request_type;
            typedef base::response_type response_type;

            typedef base::message_type message_type;
            typedef base::con_msg_manager_type con_msg_manager_type;
            typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

            typedef base::alog_type alog_type;
            typedef base::elog_type elog_type;

            typedef base::rng_type rng_type;

            struct transport_config : public base::transport_config {
                typedef type::concurrency_type concurrency_type;
                typedef type::alog_type alog_type;
                typedef type::elog_type elog_type;
                typedef type::request_type request_type;
                typedef type::response_type response_type;
                typedef websocketpp::transport::asio::basic_socket::endpoint
                    socket_type;
            };

            typedef websocketpp::transport::asio::endpoint<transport_config>
                transport_type;

            struct permessage_deflate_config {};

            typedef websocketpp::extensions::permessage_deflate::enabled
                <permessage_deflate_config> permessage_deflate_type;
        };

        /* typedefs */
        using server = websocketpp::server<asio_with_deflate>;
        using connection_hdl = websocketpp::connection_hdl;
        using message_ptr = server::message_ptr;
        using ConnectionList = std::map<connection_hdl, bool, std::owner_less<connection_hdl>>;
        using json = nlohmann::json;
        using ITrackList = musik::core::sdk::ITrackList;
        using ITrack = musik::core::sdk::ITrack;

        /* vars */
        Context& context;
        ConnectionList connections;
        ReadWriteLock connectionLock;
        std::shared_ptr<server> wss;
        std::shared_ptr<std::thread> thread;
        std::mutex exitMutex;
        std::condition_variable exitCondition;
        Snapshots snapshots;
        volatile bool running;

        /* gross extra state */
        std::string lastPlaybackOverview;

        void ThreadProc();
        void HandleAuthentication(connection_hdl connection, json& request);
        void HandleRequest(connection_hdl connection, json& request);

        void Broadcast(const std::string& name, json& options);
        void RespondWithOptions(connection_hdl connection, json& request, json& options);
        void RespondWithOptions(connection_hdl connection, json& request, json&& options = json({}));
        void RespondWithInvalidRequest(connection_hdl connection, const std::string& name, const std::string& id);
        void RespondWithSuccess(connection_hdl connection, json& request);
        void RespondWithFailure(connection_hdl connection, json& request);
        void RespondWithSuccess(connection_hdl connection, const std::string& name, const std::string& id);

        void RespondWithSetVolume(connection_hdl connection, json& request);
        void RespondWithPlaybackOverview(connection_hdl connection, json& reuest);
        bool RespondWithTracks(connection_hdl connection, json& request, ITrackList* tracks, int limit, int offset);
        void RespondWithQueryTracks(connection_hdl connection, json& request);
        void RespondWithQueryTracksByExternalIds(connection_hdl connection, json& request);
        void RespondWithPlayQueueTracks(connection_hdl connection, json& request);
        void RespondWithQueryAlbums(connection_hdl connection, json& request);
        void RespondWithPlayTracks(connection_hdl connection, json& request);
        void RespondWithQueryTracksByCategory(connection_hdl connection, json& request);
        void RespondWithListCategories(connection_hdl connection, json& request);
        void RespondWithQueryCategory(connection_hdl connection, json& request);
        void RespondWithPlayAllTracks(connection_hdl connection, json& request);
        void RespondWithPlaySnapshotTracks(connection_hdl connection, json& request);
        void RespondWithPlayTracksByCategory(connection_hdl connection, json& request);
        void RespondWithEnvironment(connection_hdl connection, json& request);
        void RespondWithCurrentTime(connection_hdl connection, json& request);
        void RespondWithSavePlaylist(connection_hdl connection, json& request);
        void RespondWithRenamePlaylist(connection_hdl connection, json& request);
        void RespondWithDeletePlaylist(connection_hdl connection, json& request);
        void RespondWithAppendToPlaylist(connection_hdl connection, json& request);
        void RespondWithRemoveTracksFromPlaylist(connection_hdl connection, json& request);
        void RespondWithRunIndexer(connection_hdl connection, json& request);
        void RespondWithListOutputDrivers(connection_hdl connection, json& request);
        void RespondWithSetDefaultOutputDriver(connection_hdl connection, json& request);
        void RespondWithGetGainSettings(connection_hdl connection, json& request);
        void RespondWithSetGainSettings(connection_hdl connection, json& request);
        void RespondWithGetEqualizerSettings(connection_hdl connection, json& request);
        void RespondWithSetEqualizerSettings(connection_hdl connection, json& request);
        void RespondWithGetTransportType(connection_hdl connection, json& request);
        void RespondWithSetTransportType(connection_hdl connection, json& request);
        void RespondWithSnapshotPlayQueue(connection_hdl connection, json& request);
        void RespondWithInvalidatePlayQueueSnapshot(connection_hdl connection, json& request);

        void BroadcastPlaybackOverview();
        void BroadcastPlayQueueChanged();

        void GetLimitAndOffset(json& options, int& limit, int& offset);
        ITrackList* QueryTracksByCategory(json& request, int& limit, int& offset);
        ITrackList* QueryTracks(json& request, int& limit, int& offset);
        json ReadTrackMetadata(ITrack* track);
        void BuildPlaybackOverview(json& options);

        void OnOpen(connection_hdl connection);
        void OnClose(connection_hdl connection);
        void OnMessage(server* s, connection_hdl hdl, message_ptr msg);
};