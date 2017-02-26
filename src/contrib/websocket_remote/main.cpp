#include <iostream>

#include <core/sdk/IPlaybackRemote.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/ISimpleDataProvider.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/server.hpp>

#include <websocketpp/server.hpp>

#include <boost/format.hpp>
#include <boost/bimap.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include <json.hpp>

#include <map>
#include <unordered_map>
#include <set>

#ifdef WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#define DEFAULT_PORT 7905
#define DEFAULT_PASSWORD ""

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


using namespace musik::core::sdk;
using namespace nlohmann;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using server = websocketpp::server<asio_with_deflate>;
using connection_hdl = websocketpp::connection_hdl;
using message_ptr = server::message_ptr;

using ConnectionList = std::map<connection_hdl, bool, std::owner_less<connection_hdl>>;

typedef boost::shared_mutex Mutex;
typedef boost::unique_lock<Mutex> WriteLock;
typedef boost::shared_lock<Mutex> ReadLock;

static Mutex stateMutex;
static IPlaybackService* playback = nullptr;
static IPreferences* preferences = nullptr;
static ISimpleDataProvider* dataProvider = nullptr;
static int nextId = 0;

#ifdef __APPLE__
__thread char threadLocalBuffer[4096];
#else
thread_local char threadLocalBuffer[4096];
#endif

namespace prefs {
    static const std::string port = "port";
}

namespace message {
    static const std::string name = "name";
    static const std::string id = "id";
    static const std::string type = "type";
    static const std::string options = "options";
}

namespace key {
    static const std::string error = "error";
    static const std::string state = "state";
    static const std::string volume = "volume";
    static const std::string position = "position";
    static const std::string repeat_mode = "repeat_mode";
    static const std::string shuffled = "shuffled";
    static const std::string muted = "muted";
    static const std::string play_queue_count = "track_count";
    static const std::string play_queue_position = "play_queue_position";
    static const std::string playing_duration = "playing_duration";
    static const std::string playing_current_time = "playing_current_time";
    static const std::string playing_track = "playing_track";
    static const std::string title = "title";
    static const std::string artist = "artist";
    static const std::string album = "album";
    static const std::string album_artist = "album_artist";
    static const std::string genre = "genre";
    static const std::string thumbnail_id = "thumbnail_id";
    static const std::string genre_id = "visual_genre_id";
    static const std::string artist_id = "visual_artist_id";
    static const std::string album_artist_id = "album_artist_id";
    static const std::string album_id = "album_id";
    static const std::string category = "category";
    static const std::string category_id = "category_id";
    static const std::string filter = "filter";
    static const std::string id = "id";
    static const std::string ids = "ids";
    static const std::string value = "value";
    static const std::string data = "data";
    static const std::string limit = "limit";
    static const std::string offset = "offset";
    static const std::string count_only = "count_only";
    static const std::string count = "count";
    static const std::string success = "success";
    static const std::string index = "index";
    static const std::string delta = "delta";
    static const std::string relative = "relative";
    static const std::string password = "password";
    static const std::string port = "port";
    static const std::string authenticated = "authenticated";
}

namespace value {
    static const std::string invalid = "invalid";
    static const std::string unauthenticated = "unauthenticated";
    static const std::string up = "up";
    static const std::string down = "down";
    static const std::string delta = "delta";
}

namespace type {
    static const std::string request = "request";
    static const std::string response = "response";
    static const std::string broadcast = "broadcast";
}

namespace request {
    static const std::string authenticate = "authenticate";
    static const std::string ping = "ping";
    static const std::string pause_or_resume = "pause_or_resume";
    static const std::string stop = "stop";
    static const std::string previous = "previous";
    static const std::string next = "next";
    static const std::string play_at_index = "play_at_index";
    static const std::string toggle_shuffle = "toggle_shuffle";
    static const std::string toggle_repeat = "toggle_repeat";
    static const std::string set_volume = "set_volume";
    static const std::string seek_to = "seek_to";
    static const std::string seek_relative = "seek_relative";
    static const std::string toggle_mute = "toggle_mute";
    static const std::string get_playback_overview = "get_playback_overview";
    static const std::string query_category = "query_category";
    static const std::string query_tracks = "query_tracks";
    static const std::string query_albums = "query_albums";
    static const std::string query_tracks_by_category = "query_tracks_by_category";
    static const std::string play_all_tracks = "play_all_tracks";
    static const std::string play_tracks = "play_tracks";
    static const std::string play_tracks_by_category = "play_tracks_by_category";
    static const std::string query_play_queue_tracks = "query_play_queue_tracks";
}

namespace broadcast {
    static const std::string playback_overview_changed = "playback_overview_changed";
    static const std::string play_queue_changed = "play_queue_changed";
}

static std::string nextMessageId() {
    return boost::str(boost::format("musikcube-server-%d") % ++nextId);
}

template <typename L, typename R>
boost::bimap<L, R>
static makeBimap(std::initializer_list<typename boost::bimap<L, R>::value_type> list) {
    /* http://stackoverflow.com/a/31841462 */
    return boost::bimap<L, R>(list.begin(), list.end());
}

static auto PLAYBACK_STATE_TO_STRING = makeBimap<PlaybackState, std::string>({
    { PlaybackStopped, "stopped" },
    { PlaybackPlaying, "playing" },
    { PlaybackPaused, "paused" }
});

static auto REPEAT_MODE_TO_STRING = makeBimap<RepeatMode, std::string>({
    { RepeatNone, "none" },
    { RepeatTrack, "track" },
    { RepeatList, "list" }
});

class Plugin : public IPlugin {
    public:
        virtual void Destroy() { }
        virtual const char* Name() { return "WebSockets IPlaybackRemote"; }
        virtual const char* Version() { return "0.0.1"; }
        virtual const char* Author() { return "clangen"; }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
};

class PlaybackRemote : public IPlaybackRemote {
    public:
        PlaybackRemote() {
        }

        virtual ~PlaybackRemote() {
            if (this->thread) {
                wss.stop();
                this->thread->join();
            }
        }

        virtual void Destroy() {
        }

        void CheckRunningStatus() {
            if (!thread && ::playback && ::preferences && ::dataProvider) {
                thread.reset(new std::thread(std::bind(&PlaybackRemote::ThreadProc, this)));
            }
            else if (thread && (!::playback || !::preferences || !dataProvider)) {
                wss.stop();
                this->thread->join();
                this->thread.reset();
            }
        }

        virtual void SetPlaybackService(IPlaybackService* playback) {
            WriteLock wl(::stateMutex);
            ::playback = playback;
            this->CheckRunningStatus();
        }

        virtual void OnTrackChanged(ITrack* track) {
            this->BroadcastPlaybackOverview();
        }

        virtual void OnPlaybackStateChanged(PlaybackState state) {
            this->BroadcastPlaybackOverview();
        }

        virtual void OnVolumeChanged(double volume) {
            this->BroadcastPlaybackOverview();
        }

        virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled) {
            this->BroadcastPlaybackOverview();
        }

        virtual void OnPlayQueueChanged() {
            this->BroadcastPlayQueueChanged();
        }

    private:
        void HandleAuthentication(connection_hdl connection, json& request) {
            std::string name = request[message::name];

            if (name == request::authenticate) {
                std::string sent = request[message::options][key::password];

                std::string actual = this->GetPreferenceString(
                    ::preferences, key::password, DEFAULT_PASSWORD);

                if (sent == actual) {
                    this->connections[connection] = true; /* mark as authed */

                    this->RespondWithOptions(
                        connection,
                        request,
                        json({ { key::authenticated, true } }));

                    return;
                }
            }

            this->wss.close(
                connection,
                websocketpp::close::status::policy_violation,
                value::unauthenticated);
        }

        void HandleRequest(connection_hdl connection, json& request) {
            if (this->connections[connection] == false) {
                this->HandleAuthentication(connection, request);
                return;
            }

            std::string name = request[message::name];
            std::string id = request[message::id];

            if (!name.size() || !id.size()) {
                RespondWithInvalidRequest(connection, name, id);
            }
            else if (::playback) {
                if (request.find(message::options) == request.end()) {
                    request[message::options] = { };
                }

                const json& options = request[message::options];

                if (name == request::ping) {
                    this->RespondWithSuccess(connection, request);
                    return;
                }
                if (name == request::pause_or_resume) {
                    playback->PauseOrResume();
                    return;
                }
                else if (name == request::stop) {
                    playback->Stop();
                    return;
                }
                else if (name == request::previous) {
                    playback->Previous();
                    return;
                }
                else if (name == request::next) {
                    playback->Next();
                    return;
                }
                else if (name == request::play_at_index) {
                    if (options.find(key::index) != options.end()) {
                        playback->Play(options[key::index]);
                    }
                    return;
                }
                else if (name == request::toggle_shuffle) {
                    playback->ToggleShuffle();
                    return;
                }
                else if (name == request::toggle_repeat) {
                    playback->ToggleRepeatMode();
                    return;
                }
                else if (name == request::toggle_mute) {
                    playback->ToggleMute();
                    return;
                }
                else if (name == request::set_volume) {
                    this->RespondWithSetVolume(connection, request);
                    return;
                }
                else if (name == request::seek_to) {
                    if (options.find(key::position) != options.end()) {
                        playback->SetPosition(options[key::position]);
                    }
                    return;
                }
                else if (name == request::seek_relative) {
                    double delta = options.value(key::delta, 0.0f);
                    if (delta != 0.0f) {
                        playback->SetPosition(playback->GetPosition() + delta);
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
            }

            this->RespondWithInvalidRequest(connection, name, id);
        }

        void Broadcast(const std::string& name, json& options) {
            json msg;
            msg[message::name] = name;
            msg[message::type] = type::broadcast;
            msg[message::id] = nextMessageId();
            msg[message::options] = options;

            std::string str = msg.dump();

            ReadLock rl(::stateMutex);
            for (const auto &keyValue : this->connections) {
                wss.send(keyValue.first, str.c_str(), websocketpp::frame::opcode::text);
            }
        }

        void RespondWithOptions(connection_hdl connection, json& request, json& options) {
            json response = {
                { message::name, request[message::name] },
                { message::type, type::response },
                { message::id, request[message::id] },
                { message::options, options }
            };

            wss.send(connection, response.dump().c_str(), websocketpp::frame::opcode::text);
        }

        void RespondWithOptions(connection_hdl connection, json& request, json&& options = json({})) {
            json response = {
                { message::name, request[message::name] },
                { message::type, type::response },
                { message::id, request[message::id] },
                { message::options, options }
            };

            wss.send(connection, response.dump().c_str(), websocketpp::frame::opcode::text);
        }

        void RespondWithInvalidRequest(
            connection_hdl connection,
            const std::string& name,
            const std::string& id)
        {
            json error = {
                { message::name, name },
                { message::id, id },
                { message::options, {
                    { key::error, value::invalid }
                } }
            };
            wss.send(connection, error.dump().c_str(), websocketpp::frame::opcode::text);
        }

        void RespondWithSuccess(connection_hdl connection, json& request) {
            std::string name = request[message::name];
            std::string id = request[message::id];
            this->RespondWithSuccess(connection, name, id);
        }

        void RespondWithSuccess(
            connection_hdl connection,
            const std::string& name,
            const std::string& id)
        {
            json error = {
                { message::name, name },
                { message::id, id },
                { message::type, type::response },
                { message::options, { key::success, true } }
            };

            wss.send(connection, error.dump().c_str(), websocketpp::frame::opcode::text);
        }

        void RespondWithSetVolume(connection_hdl connection, json& request) {
            json& options = request[message::options];
            std::string relative = options.value(key::relative, "");

            if (relative == value::up) {
                double delta = round(playback->GetVolume() * 100.0) >= 10.0 ? 0.05 : 0.01;
                playback->SetVolume(playback->GetVolume() + delta);
            }
            else if (relative == value::down) {
                double delta = round(playback->GetVolume() * 100.0) > 10.0 ? 0.05 : 0.01;
                playback->SetVolume(playback->GetVolume() - delta);
            }
            else if (relative == value::delta) {
                float delta = options[key::volume];
                playback->SetVolume(playback->GetVolume() + delta);
            }
            else {
                playback->SetVolume(options[key::volume]);
            }
        }

        void RespondWithPlaybackOverview(connection_hdl connection, json& request) {
            json options;
            this->BuildPlaybackOverview(options);
            this->RespondWithOptions(connection, request, options);
        }

        bool RespondWithTracks(
            connection_hdl connection,
            json& request,
            ITrackList* tracks,
            int limit,
            int offset)
        {
            json& options = request[message::options];
            bool countOnly = options.value(key::count_only, false);

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

                    IRetainedTrack* track;
                    for (int i = 0; i < (int)tracks->Count(); i++) {
                        track = tracks->GetRetainedTrack((size_t)i);
                        data.push_back(this->ReadTrackMetadata(track));
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

        void GetLimitAndOffset(json& options, int& limit, int& offset) {
            int optionsLimit = options.value(key::limit, -1);
            int optionsOffset = options.value(key::offset, 0);
            if (optionsLimit != -1 && optionsOffset >= 0) {
                limit = optionsLimit;
                offset = optionsOffset;
            }
        }

        void RespondWithQueryTracks(connection_hdl connection, json& request) {
            if (request.find(message::options) != request.end()) {
                json& options = request[message::options];

                std::string filter = options.value(key::filter, "");

                int limit = -1, offset = 0;
                this->GetLimitAndOffset(options, limit, offset);

                ITrackList* tracks = dataProvider->QueryTracks(filter.c_str(), limit, offset);

                if (this->RespondWithTracks(connection, request, tracks, limit, offset)) {
                    return;
                }
            }

            this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        }

        void RespondWithPlayQueueTracks(connection_hdl connection, json& request) {
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
                    { key::count, ::playback->Count() }
                });
            }
            else {
                static auto deleter = [](IRetainedTrack* track) { track->Release(); };
                std::vector<std::shared_ptr<IRetainedTrack>> tracks;

                /* edit the playlist so it can be changed while we're getting the tracks
                out of it. */
                ITrackListEditor* editor = ::playback->EditPlaylist();

                int trackCount = (int) ::playback->Count();
                int to = std::min(trackCount, offset + limit);

                for (int i = offset; i < to; i++) {
                    tracks.push_back(std::shared_ptr<IRetainedTrack>(::playback->GetTrack(i), deleter));
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

        void RespondWithQueryAlbums(connection_hdl connection, json& request) {
            if (request.find(message::options) != request.end()) {
                json& options = request[message::options];

                std::string filter = options.value(key::filter, "");
                std::string category = options.value(key::category, "");
                unsigned long long categoryId = options.value(key::category_id, -1);

                IMetadataMapList* albumList = dataProvider
                    ->QueryAlbums(category.c_str(), categoryId, filter.c_str());

                json result = json::array();

                IMetadataMap* album;
                for (size_t i = 0; i < albumList->Count(); i++) {
                    album = albumList->GetMetadata(i);

                    result.push_back({
                        { key::title, album->GetDescription() },
                        { key::id, album->GetId() },
                        { key::thumbnail_id, 0 }, /* note: thumbnails aren't supported at the album level yet */
                        { key::album_artist_id, this->GetMetadataInt64(album, key::album_artist_id) },
                        { key::album_artist, this->GetMetadataString(album, key::album_artist) }
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

        void RespondWithPlayTracks(connection_hdl connection, json& request) {
            if (request.find(message::options) != request.end()) {
                json& options = request[message::options];

                if (options.find(key::ids) != options.end()) {
                    json& ids = options[key::ids];
                    if (ids.is_array()) {
                        size_t count = 0;
                        ITrackListEditor* editor = ::playback->EditPlaylist();
                        editor->Clear();

                        for (auto it = ids.begin(); it != ids.end(); ++it) {
                            editor->Add(*it);
                            ++count;
                        }

                        editor->Release();

                        if (count > 0) {
                            ::playback->Play(0);
                            this->RespondWithSuccess(connection, request);
                            return;
                        }
                    }
                }
            }

            this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        }

        ITrackList* QueryTracksByCategory(json& request, int& limit, int& offset) {
            if (request.find(message::options) != request.end()) {
                json& options = request[message::options];

                std::string category = options[key::category];
                unsigned long long selectedId = options[key::id];

                std::string filter = options.value(key::filter, "");

                limit = -1, offset = 0;
                this->GetLimitAndOffset(options, limit, offset);

                return dataProvider->QueryTracksByCategory(
                    category.c_str(), selectedId, filter.c_str(), limit, offset);
            }

            return nullptr;
        }

        void RespondWithQueryTracksByCategory(connection_hdl connection, json& request) {
            int limit, offset;
            ITrackList* tracks = QueryTracksByCategory(request, limit, offset);

            if (tracks && this->RespondWithTracks(connection, request, tracks, limit, offset)) {
                return;
            }

            this->RespondWithInvalidRequest(connection, request[message::name], value::invalid);
        }

        void RespondWithQueryCategory(connection_hdl connection, json& request) {
            if (request.find(message::options) != request.end()) {
                std::string category = request[message::options][key::category];
                std::string filter = request[message::options].value(key::filter, "");

                if (category.size()) {
                    IMetadataValueList* result = dataProvider
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

        void RespondWithPlayAllTracks(connection_hdl connection, json& request) {
            size_t index = 0;
            std::string filter;

            if (request.find(message::options) != request.end()) {
                index = request[message::options].value(key::index, 0);
                filter = request[message::options].value(key::filter, "");
            }

            ITrackList* tracks = ::dataProvider->QueryTracks(filter.c_str());

            if (tracks) {
                ::playback->Play(tracks, index);
                tracks->Release();
            }

            RespondWithSuccess(connection, request);
        }

        void RespondWithPlayTracksByCategory(connection_hdl connection, json& request) {
            int limit, offset;
            ITrackList* tracks = this->QueryTracksByCategory(request, limit, offset);

            if (tracks) {
                size_t index = request[message::options].value(key::index, 0);
                ::playback->Play(tracks, index);
                tracks->Release();
            }

            this->RespondWithSuccess(connection, request);
        }

        void BroadcastPlaybackOverview() {
            {
                ReadLock rl(::stateMutex);
                if (!this->connections.size()) {
                    return;
                }
            }

            json options;
            this->BuildPlaybackOverview(options);
            this->Broadcast(broadcast::playback_overview_changed, options);
        }

        void BroadcastPlayQueueChanged() {
            {
                ReadLock rl(::stateMutex);
                if (!this->connections.size()) {
                    return;
                }
            }

            json options;
            this->Broadcast(broadcast::play_queue_changed, options);
        }

        json ReadTrackMetadata(IRetainedTrack* track) {
            return {
                { key::id, track->GetId() },
                { key::title, this->GetMetadataString(track, key::title) },
                { key::album, this->GetMetadataString(track, key::album) },
                { key::album_id, this->GetMetadataInt64(track, key::album_id) },
                { key::album_artist, this->GetMetadataString(track, key::album_artist) },
                { key::album_artist_id, this->GetMetadataInt64(track, key::album_artist_id) },
                { key::artist, this->GetMetadataString(track, key::artist) },
                { key::artist_id, this->GetMetadataInt64(track, key::artist_id) },
                { key::genre, this->GetMetadataString(track, key::genre) },
                { key::genre_id, this->GetMetadataInt64(track, key::genre_id) },
                { key::thumbnail_id, this->GetMetadataInt64(track, key::thumbnail_id) },
            };
        }

        void BuildPlaybackOverview(json& options) {
            options[key::state] = PLAYBACK_STATE_TO_STRING.left.find(::playback->GetPlaybackState())->second;
            options[key::repeat_mode] = REPEAT_MODE_TO_STRING.left.find(::playback->GetRepeatMode())->second;
            options[key::volume] = ::playback->GetVolume();
            options[key::shuffled] = ::playback->IsShuffled();
            options[key::muted] = ::playback->IsMuted();
            options[key::play_queue_count] = ::playback->Count();
            options[key::play_queue_position] = ::playback->GetIndex();
            options[key::playing_duration] = ::playback->GetDuration();
            options[key::playing_current_time] = ::playback->GetPosition();

            IRetainedTrack* track = ::playback->GetPlayingTrack();
            if (track) {
                options[key::playing_track] = this->ReadTrackMetadata(track);
                track->Release();
            }
        }

        std::string GetPreferenceString(
            IPreferences* prefs,
            const std::string& key,
            const std::string& defaultValue)
        {
            prefs->GetString(key.c_str(), threadLocalBuffer, sizeof(threadLocalBuffer), defaultValue.c_str());
            return std::string(threadLocalBuffer);
        }

        template <typename MetadataT>
        std::string GetMetadataString(MetadataT* metadata, const std::string& key) {
            metadata->GetValue(key.c_str(), threadLocalBuffer, sizeof(threadLocalBuffer));
            return std::string(threadLocalBuffer);
        }

        template <typename MetadataT>
        unsigned long long GetMetadataInt64(MetadataT* metadata, const std::string& idKey) {
            try {
                return std::stoull(this->GetMetadataString(metadata, idKey));
            }
            catch (...) {
                return -1;
            }
        }

        void ThreadProc() {
            try {
                if (preferences->GetBool("debug")) {
                    wss.get_alog().set_ostream(&std::cerr);
                    wss.get_elog().set_ostream(&std::cerr);
                    wss.set_access_channels(websocketpp::log::alevel::all);
                    wss.clear_access_channels(websocketpp::log::alevel::frame_payload);
                }
                else {
                    wss.set_access_channels(websocketpp::log::alevel::none);
                    wss.clear_access_channels(websocketpp::log::alevel::none);
                }

                wss.init_asio();
                wss.set_message_handler(std::bind(&PlaybackRemote::OnMessage, this, &wss, ::_1, ::_2));
                wss.set_open_handler(std::bind(&PlaybackRemote::OnOpen, this, ::_1));
                wss.set_close_handler(std::bind(&PlaybackRemote::OnClose, this, ::_1));
                wss.listen(preferences->GetInt(prefs::port.c_str(), DEFAULT_PORT));
                wss.start_accept();

                wss.run();
            }
            catch (websocketpp::exception const & e) {
                std::cerr << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "other exception" << std::endl;
            }
        }

        void OnOpen(connection_hdl connection) {
            WriteLock wl(::stateMutex);
            connections[connection] = false;
        }

        void OnClose(connection_hdl connection) {
            WriteLock wl(::stateMutex);
            connections.erase(connection);
        }

        void OnMessage(server* s, connection_hdl hdl, message_ptr msg) {
            ReadLock rl(::stateMutex);

            try {
                json data = json::parse(msg->get_payload());
                std::string type = data[message::type];
                if (type == type::request) {
                    this->HandleRequest(hdl, data);
                }
            }
            catch (...) {
                std::cerr << "message parse failed: " << msg->get_payload() << "\n";
                this->RespondWithInvalidRequest(hdl, value::invalid, value::invalid);
            }
        }

        ConnectionList connections;
        std::shared_ptr<std::thread> thread;
        server wss;

};

static Plugin plugin;
static PlaybackRemote remote;

extern "C" DLL_EXPORT IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" DLL_EXPORT IPlaybackRemote* GetPlaybackRemote() {
    return &remote;
}

extern "C" DLL_EXPORT void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    WriteLock wl(::stateMutex);
    ::preferences = prefs;

    if (prefs) {
        prefs->GetInt(key::port.c_str(), DEFAULT_PORT);
        prefs->GetString(key::password.c_str(), nullptr, 0, DEFAULT_PASSWORD);
        prefs->Save();
    }

    remote.CheckRunningStatus();
}

extern "C" DLL_EXPORT void SetSimpleDataProvider(musik::core::sdk::ISimpleDataProvider* dataProvider) {
    WriteLock wl(::stateMutex);
    ::dataProvider = dataProvider;
    remote.CheckRunningStatus();
}
