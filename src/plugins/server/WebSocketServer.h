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

///

/// @file WebSocketServer.h
/// @brief WebSocket server for remote control and streaming.
/// @details Built on websocketpp with permessage-deflate support. Serves the
/// remote control protocol (JSON requests/responses/broadcasts), pushes
/// playback state updates to connected clients and maintains play-queue
/// snapshots so clients can re-sync after reconnecting.

#include "Context.h"
#include "Snapshots.h"

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/ITrack.h>

#pragma warning(push, 0)
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#pragma warning(pop, 0)

#include <mutex>
#include <condition_variable>

/** @brief WebSocket server implementing the musikcube remote protocol.
 *  @details Accepts JSON messages over WebSocket, dispatches them to handler
 *  methods, and broadcasts playback/play-queue changes to all connected
 *  clients. Optional password authentication guards the connection. */
class WebSocketServer {
    public:
        /** @brief Constructs a server bound to the shared context.
         *  @param context Shared server context. */
        WebSocketServer(Context& context);
        /** @brief Destroys the server, stopping it if running. */
        ~WebSocketServer();

        /** @brief Starts the WebSocket server.
         *  @return True if the server started. */
        bool Start();
        /** @brief Stops the WebSocket server.
         *  @return True if the server was stopped. */
        bool Stop();
        /** @brief Blocks until the server thread exits. */
        void Wait();

        /** @brief Called when the current track changes.
         *  @param track The new track. */
        void OnTrackChanged(musik::core::sdk::ITrack* track);
        /** @brief Called when the playback state changes.
         *  @param state The new playback state. */
        void OnPlaybackStateChanged(musik::core::sdk::PlaybackState state);
        /** @brief Called when the playback position changes.
         *  @param time Position in seconds. */
        void OnPlaybackTimeChanged(double time);
        /** @brief Called when the volume changes.
         *  @param volume The new volume, 0.0 to 1.0. */
        void OnVolumeChanged(double volume);
        /** @brief Called when repeat/shuffle mode changes.
         *  @param repeatMode The new repeat mode.
         *  @param shuffled Whether shuffle is enabled. */
        void OnModeChanged(musik::core::sdk::RepeatMode repeatMode, bool shuffled);
        /** @brief Called when the play queue changes. */
        void OnPlayQueueChanged();

    private:
        /* our special server config that supports gzip */
        /** @brief websocketpp server config with permessage-deflate enabled. */
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

            /** @brief Transport config wiring the socket and deflate types. */
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

            /** @brief Permessage-deflate extension configuration. */
            struct permessage_deflate_config {};

            typedef websocketpp::extensions::permessage_deflate::enabled
                <permessage_deflate_config> permessage_deflate_type;
        };

        /* typedefs */
        /** @brief The configured websocketpp server type. */
        using server = websocketpp::server<asio_with_deflate>;
        /** @brief Connection handle alias. */
        using connection_hdl = websocketpp::connection_hdl;
        /** @brief Message pointer alias. */
        using message_ptr = server::message_ptr;
        /** @brief Map of live connections to their state. */
        using ConnectionList = std::map<connection_hdl, bool, std::owner_less<connection_hdl>>;
        /** @brief JSON type alias. */
        using json = nlohmann::json;
        /** @brief Track list type alias. */
        using ITrackList = musik::core::sdk::ITrackList;
        /** @brief Track type alias. */
        using ITrack = musik::core::sdk::ITrack;

        /* vars */
        /** @brief Shared server context. */
        Context& context;
        /** @brief Currently connected clients. */
        ConnectionList connections;
        /** @brief Guards the connection list. */
        ReadWriteLock connectionLock;
        /** @brief The websocketpp server endpoint. */
        std::shared_ptr<server> wss;
        /** @brief The server dispatch thread. */
        std::shared_ptr<std::thread> thread;
        /** @brief Guards the exit condition. */
        std::mutex exitMutex;
        /** @brief Signals the thread when the server stops. */
        std::condition_variable exitCondition;
        /** @brief Play-queue snapshots for reconnecting clients. */
        Snapshots snapshots;
        /** @brief Whether the server is running. */
        volatile bool running;

        /* gross extra state */
        /** @brief Cached last playback overview for quick re-broadcasts. */
        std::string lastPlaybackOverview;

        /** @brief WebSocket dispatch loop body. */
        void ThreadProc();
        /** @brief Handles an authenticate request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void HandleAuthentication(connection_hdl connection, json& request);
        /** @brief Dispatches a request JSON to its handler.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void HandleRequest(connection_hdl connection, json& request);

        /** @brief Sends a broadcast message to all connected clients.
         *  @param name The broadcast name.
         *  @param options The broadcast payload. */
        void Broadcast(const std::string& name, json& options);
        /** @brief Responds to a request with a JSON options payload.
         *  @param connection The connection.
         *  @param request The request JSON.
         *  @param options The response payload. */
        void RespondWithOptions(connection_hdl connection, json& request, json& options);
        /** @brief Responds to a request with a JSON options payload (rvalue).
         *  @param connection The connection.
         *  @param request The request JSON.
         *  @param options The response payload. */
        void RespondWithOptions(connection_hdl connection, json& request, json&& options = json({}));
        /** @brief Responds with an invalid-request error.
         *  @param connection The connection.
         *  @param name The request name.
         *  @param id The request id. */
        void RespondWithInvalidRequest(connection_hdl connection, const std::string& name, const std::string& id);
        /** @brief Responds with a success message.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSuccess(connection_hdl connection, json& request);
        /** @brief Responds with a failure message.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithFailure(connection_hdl connection, json& request);
        /** @brief Responds with a success message.
         *  @param connection The connection.
         *  @param name The request name.
         *  @param id The request id. */
        void RespondWithSuccess(connection_hdl connection, const std::string& name, const std::string& id);

        /** @brief Handles the send_raw_query request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSendRawQuery(connection_hdl connection, json& request);
        /** @brief Handles the set_volume request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSetVolume(connection_hdl connection, json& request);
        /** @brief Handles the get_playback_overview request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlaybackOverview(connection_hdl connection, json& request);
        /** @brief Sends a track list response with paging.
         *  @param connection The connection.
         *  @param request The request JSON.
         *  @param tracks The track list to serialize.
         *  @param limit Maximum number of tracks.
         *  @param offset Page offset.
         *  @return True if the response was sent. */
        bool RespondWithTracks(connection_hdl connection, json& request, ITrackList* tracks, int limit, int offset);
        /** @brief Handles the query_tracks request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithQueryTracks(connection_hdl connection, json& request);
        /** @brief Handles the query_tracks_by_external_ids request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithQueryTracksByExternalIds(connection_hdl connection, json& request);
        /** @brief Handles the query_play_queue_tracks request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlayQueueTracks(connection_hdl connection, json& request);
        /** @brief Handles the query_albums request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithQueryAlbums(connection_hdl connection, json& request);
        /** @brief Handles the play_tracks request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlayTracks(connection_hdl connection, json& request);
        /** @brief Handles the query_tracks_by_category request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithQueryTracksByCategory(connection_hdl connection, json& request);
        /** @brief Handles the list_categories request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithListCategories(connection_hdl connection, json& request);
        /** @brief Handles the query_category request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithQueryCategory(connection_hdl connection, json& request);
        /** @brief Handles the play_all_tracks request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlayAllTracks(connection_hdl connection, json& request);
        /** @brief Handles the play_snapshot_tracks request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlaySnapshotTracks(connection_hdl connection, json& request);
        /** @brief Handles the play_tracks_by_category request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithPlayTracksByCategory(connection_hdl connection, json& request);
        /** @brief Handles the get_environment request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithEnvironment(connection_hdl connection, json& request);
        /** @brief Handles the get_current_time request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithCurrentTime(connection_hdl connection, json& request);
        /** @brief Handles the save_playlist request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSavePlaylist(connection_hdl connection, json& request);
        /** @brief Handles the rename_playlist request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithRenamePlaylist(connection_hdl connection, json& request);
        /** @brief Handles the delete_playlist request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithDeletePlaylist(connection_hdl connection, json& request);
        /** @brief Handles the append_to_playlist request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithAppendToPlaylist(connection_hdl connection, json& request);
        /** @brief Handles the remove_tracks_from_playlist request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithRemoveTracksFromPlaylist(connection_hdl connection, json& request);
        /** @brief Handles the run_indexer request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithRunIndexer(connection_hdl connection, json& request);
        /** @brief Handles the list_output_drivers request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithListOutputDrivers(connection_hdl connection, json& request);
        /** @brief Handles the set_default_output_driver request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSetDefaultOutputDriver(connection_hdl connection, json& request);
        /** @brief Handles the get_gain_settings request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithGetGainSettings(connection_hdl connection, json& request);
        /** @brief Handles the set_gain_settings request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSetGainSettings(connection_hdl connection, json& request);
        /** @brief Handles the get_equalizer_settings request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithGetEqualizerSettings(connection_hdl connection, json& request);
        /** @brief Handles the set_equalizer_settings request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSetEqualizerSettings(connection_hdl connection, json& request);
        /** @brief Handles the get_transport_type request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithGetTransportType(connection_hdl connection, json& request);
        /** @brief Handles the set_transport_type request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSetTransportType(connection_hdl connection, json& request);
        /** @brief Handles the snapshot_play_queue request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithSnapshotPlayQueue(connection_hdl connection, json& request);
        /** @brief Handles the invalidate_play_queue_snapshot request.
         *  @param connection The connection.
         *  @param request The request JSON. */
        void RespondWithInvalidatePlayQueueSnapshot(connection_hdl connection, json& request);

        /** @brief Broadcasts the current playback overview to all clients. */
        void BroadcastPlaybackOverview();
        /** @brief Broadcasts a play-queue changed event to all clients. */
        void BroadcastPlayQueueChanged();

        /** @brief Extracts limit and offset from a request's options.
         *  @param options The options JSON.
         *  @param limit Receives the limit.
         *  @param offset Receives the offset. */
        void GetLimitAndOffset(json& options, int& limit, int& offset);
        /** @brief Queries tracks by category with paging.
         *  @param request The request JSON.
         *  @param limit Receives the limit.
         *  @param offset Receives the offset.
         *  @return The resulting track list. */
        ITrackList* QueryTracksByCategory(json& request, int& limit, int& offset);
        /** @brief Queries tracks with paging.
         *  @param request The request JSON.
         *  @param limit Receives the limit.
         *  @param offset Receives the offset.
         *  @return The resulting track list. */
        ITrackList* QueryTracks(json& request, int& limit, int& offset);
        /** @brief Serializes a track's metadata to JSON.
         *  @param track The track to serialize.
         *  @return The metadata JSON. */
        json ReadTrackMetadata(ITrack* track);
        /** @brief Builds the playback overview into the given options JSON.
         *  @param options The JSON to populate. */
        void BuildPlaybackOverview(json& options);

        /** @brief Called when a client connects.
         *  @param connection The connection. */
        void OnOpen(connection_hdl connection);
        /** @brief Called when a client disconnects.
         *  @param connection The connection. */
        void OnClose(connection_hdl connection);
        /** @brief Called when a message is received from a client.
         *  @param s The server endpoint.
         *  @param hdl The connection handle.
         *  @param msg The received message. */
        void OnMessage(server* s, connection_hdl hdl, message_ptr msg);
};