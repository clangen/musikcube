#include "PlaybackRemotes.h"

#include "Plugins.h"
#include <common/Contract.h>
#include <common/runtime/IMessageQueue.h>
#include <common/runtime/Message.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/plugin/PluginLibraryService.h>
#include <musikcore/plugin/indexer.h>
#include <musikcore/debug.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/support/Common.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace musik { namespace core { namespace plugin {

    class PlaybackRemotes::Remote : private runtime::IMessageTarget {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(Remote);

        Remote(mc_context& context, LibraryBackend::Shared local, audio::PlaybackService& playback, runtime::IMessageQueue& queue,
            ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory, const std::string& name,
            const std::string& id)
        : factory(std::move(factory))
        , id(id)
        , preferences(ReadSettingsService(name))
        , directory(GetDataDirectory())
        , queue(queue)
        , service(playback)
        , context(context)
        , libraryService(context, std::move(local), this->factory)
        {
            this->services.preferences = &this->preferences;
            this->services.media = &Media();
            this->services.audio_scratch = &AudioScratch();
            this->services.host = this->factory.Host();
            this->services.data_directory = { this->directory.data(), this->directory.size() };
            this->playbackService.context = this;
            this->playbackService.read_state = ReadState;
            this->playbackService.control_async = ControlAsync;
            this->playbackService.with_playback_async = WithPlaybackAsync;
            this->indexerService.context = this;
            this->indexerService.schedule_async = ScheduleIndexerAsync;
            this->services.indexer = &this->indexerService;
            this->services.playback = &this->playbackService;
            this->services.library = &this->libraryService.Api();

            if (playback.OpenHandle(&this->playback) != MC_OK) {
                throw std::runtime_error("could not open playback for remote");
            }
            try {
                this->queue.Register(this);
                this->registered = true;
                const auto status = this->factory.Invoke([&](const auto& api) -> mc_result {
                    mc_result created = MC_ERROR_INTERNAL;
                    try {
                        created = api.create(api.context, &this->services, &this->instance, nullptr);
                    }
                    catch (...) {
                        /* a throwing factory may have published an invalid instance. */
                        musik::contract::Assert(false, "playback remote creation threw");
                    }
                    musik::contract::Assert(created == MC_OK || !this->instance,
                        "failed playback remote factory returned an instance");
                    if (created != MC_OK) {
                        return created;
                    }
                    musik::contract::Assert(this->instance, "playback remote factory returned no instance");
                    {
                        std::lock_guard lock(this->commandMutex);
                        this->accepting = true;
                    }
                    this->started = true;
                    try {
                        return api.instance_api->start(this->instance, nullptr);
                    }
                    catch (...) {
                        /* start may have launched workers that cannot be safely inspected. */
                        musik::contract::Assert(false, "playback remote start threw");
                        return MC_ERROR_INTERNAL;
                    }
                });
                if (status != MC_OK || mc_playback_set_changed_callback(this->playback, Changed, this, nullptr, 0) != MC_OK ||
                    mc_playback_set_queue_changed_callback(this->playback, QueueChanged, this) != MC_OK)
                {
                    throw std::runtime_error("could not start playback remote");
                }
            }
            catch (...) {
                this->Stop();
                throw;
            }
        }

        ~Remote() { this->Stop(); }

        std::string_view Id() const { return this->id; }

        void RequestStop() noexcept {
            this->libraryService.CancelReads();
            {
                std::lock_guard lock(this->commandMutex);
                this->accepting = false;
            }
            if (this->stopRequested) {
                return;
            }
            this->stopRequested = true;

            if (this->playback) {
                const auto changed = mc_playback_set_changed_callback(this->playback, nullptr, nullptr, nullptr, 0);
                const auto queue = mc_playback_set_queue_changed_callback(this->playback, nullptr, nullptr);
                musik::contract::Assert(changed == MC_OK && queue == MC_OK,
                    "playback remote callbacks could not be detached");
            }
            if (this->instance && this->started) {
                try {
                    const auto status = this->factory.Invoke([&](const auto& api) {
                        api.instance_api->request_stop(this->instance);
                        return MC_OK;
                    });
                    musik::contract::Assert(status == MC_OK, "playback remote stop failed");
                }
                catch (...) {
                    musik::contract::Assert(false, "playback remote stop threw");
                }
            }
            if (this->registered) {
                this->queue.Unregister(this);
                this->registered = false;
            }
        }

        void CancelPending() noexcept {
            bool hasCommands;
            {
                std::lock_guard lock(this->commandMutex);
                hasCommands = !this->commands.empty();
            }
            if (hasCommands) {
                try {
                    const auto status = this->factory.Invoke([&](const auto&) {
                        /* a worker may be waiting for a queued command. */
                        while (true) {
                            Command command {};
                            {
                                std::lock_guard lock(this->commandMutex);
                                if (this->commands.empty()) {
                                    break;
                                }
                                command = this->commands.front();
                                this->commands.pop_front();
                            }
                            if (command.access) {
                                command.access(command.userData, MC_ERROR_CANCELLED, nullptr);
                            }
                            else {
                                command.completion(command.userData, MC_ERROR_CANCELLED);
                            }
                        }
                        return MC_OK;
                    });
                    musik::contract::Assert(status == MC_OK, "playback remote command cancellation failed");
                }
                catch (...) {
                    musik::contract::Assert(false, "playback remote command cancellation threw");
                }
            }
        }

        void Stop() noexcept {
            this->RequestStop();
            this->CancelPending();
            if (this->instance) {
                try {
                    const auto status = this->factory.Invoke([&](const auto& api) {
                        if (this->started) {
                            const auto joined = api.instance_api->join(this->instance, nullptr);
                            musik::contract::Assert(joined == MC_OK, "playback remote failed to join");
                        }
                        api.instance_api->destroy(this->instance);
                        return MC_OK;
                    });
                    musik::contract::Assert(status == MC_OK, "playback remote cleanup failed");
                    this->instance = nullptr;
                }
                catch (...) {
                    musik::contract::Assert(false, "playback remote cleanup threw");
                }
            }
            mc_playback_destroy(&this->playback);
        }

    private:
        struct Command {
            mc_playback_command command;
            double value;
            mc_playback_remote_completion_fn completion;
            mc_playback_remote_access_fn access;
            void* userData;
            bool indexer { false };
            mc_indexer_scan scan { MC_INDEXER_SCAN_ALL };
        };

        static constexpr int CommandMessage = 1;

        static mc_result MC_CALL ReadState(void* context, mc_playback_remote_state_fn read, void* userData) {
            if (!context || !read) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            auto& self = *static_cast<Remote*>(context);
            mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
            const auto status = mc_playback_get_snapshot(self.playback, &snapshot);
            if (status != MC_OK) {
                return status;
            }
            mc_track_ref* track = nullptr;
            const auto trackStatus = mc_playback_get_current_track(self.playback, &track);
            if (trackStatus != MC_OK && trackStatus != MC_ERROR_NOT_FOUND) {
                return trackStatus;
            }
            try {
                self.factory.Invoke([&](const auto&) {
                    read(userData, &snapshot, track);
                    return MC_OK;
                });
            }
            catch (...) {
                musik::contract::Assert(false, "playback remote state callback threw");
            }
            mc_track_ref_destroy(&track);
            return MC_OK;
        }

        static mc_result MC_CALL ControlAsync(void* context, mc_playback_command command, double value,
            mc_playback_remote_completion_fn completion, void* userData)
        {
            if (!context || !completion || command < MC_PLAYBACK_TOGGLE_PAUSE || command > MC_PLAYBACK_RESUME ||
                ((command == MC_PLAYBACK_SET_POSITION || command == MC_PLAYBACK_SET_VOLUME) &&
                    (!std::isfinite(value) || value < 0 || (command == MC_PLAYBACK_SET_VOLUME && value > 1))))
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&]() -> mc_result {
                auto& self = *static_cast<Remote*>(context);
                return self.Enqueue({
                    .command = command,
                    .value = value,
                    .completion = completion,
                    .access = nullptr,
                    .userData = userData,
                });
            });
        }

        static mc_result MC_CALL WithPlaybackAsync(void* context, mc_playback_remote_access_fn completion, void* userData) {
            if (!context || !completion) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&]() -> mc_result {
                auto& self = *static_cast<Remote*>(context);
                return self.Enqueue({
                    .command = {},
                    .value = 0,
                    .completion = nullptr,
                    .access = completion,
                    .userData = userData,
                });
            });
        }

        static mc_result MC_CALL ScheduleIndexerAsync(void* context, mc_indexer_scan scan,
            mc_plugin_indexer_completion_fn completion, void* userData)
        {
            if (!context || !completion || scan > MC_INDEXER_REBUILD) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&]() -> mc_result {
                auto& self = *static_cast<Remote*>(context);
                return self.Enqueue({
                    .command = {},
                    .value = 0,
                    .completion = completion,
                    .access = nullptr,
                    .userData = userData,
                    .indexer = true,
                    .scan = scan,
                });
            });
        }

        mc_result Enqueue(Command command) {
            std::lock_guard lock(this->commandMutex);
            if (!this->accepting) {
                return MC_ERROR_CANCELLED;
            }
            if (this->commands.size() >= MC_PLAYBACK_REMOTE_MAX_PENDING_COMMANDS) {
                return MC_ERROR_QUEUE_FULL;
            }
            this->commands.push_back(command);
            try {
                this->queue.Post(runtime::Message::Create(this, CommandMessage));
            }
            catch (...) {
                this->commands.pop_back();
                throw;
            }
            return MC_OK;
        }

        void ProcessMessage(runtime::IMessage&) override {
            Command command;
            {
                std::lock_guard lock(this->commandMutex);
                if (this->commands.empty()) {
                    return;
                }
                command = this->commands.front();
                this->commands.pop_front();
            }
            mc_playback* handle = nullptr;
            mc_result result;
            if (command.access) {
                result = this->service.OpenHandle(&handle);
            }
            else if (command.indexer) {
                result = mc_indexer_schedule(&this->context, command.scan);
            }
            else {
                result = mc_playback_control(this->playback, command.command, command.value);
            }
            try {
                this->factory.Invoke([&](const auto&) {
                    if (command.access) {
                        command.access(command.userData, result, handle);
                    }
                    else {
                        command.completion(command.userData, result);
                    }
                    return MC_OK;
                });
            }
            catch (...) {
                musik::contract::Assert(false, "playback remote command completion threw");
            }
        }

        static void MC_CALL Changed(void* context, mc_playback_event change) {
            auto& self = *static_cast<Remote*>(context);
            mc_plugin_playback_event event = MC_PLUGIN_PLAYBACK_EVENT_DEFAULT;
            event.event = change;
            if (mc_playback_get_snapshot(self.playback, &event.playback) != MC_OK) {
                return;
            }
            mc_track_ref* track = nullptr;
            if (change == MC_PLAYBACK_CHANGED_TRACK && mc_playback_get_current_track(self.playback, &track) == MC_OK) {
                event.track = track;
            }
            try {
                self.factory.Invoke([&](const auto& api) {
                    api.instance_api->changed(self.instance, &event);
                    return MC_OK;
                });
            }
            catch (...) {
                musik::contract::Assert(false, "playback remote notification threw");
            }
            mc_track_ref_destroy(&track);
        }

        static void MC_CALL QueueChanged(void* context) {
            auto& self = *static_cast<Remote*>(context);
            try {
                self.factory.Invoke([&](const auto& api) {
                    api.instance_api->queue_changed(self.instance);
                    return MC_OK;
                });
            }
            catch (...) {
                musik::contract::Assert(false, "playback remote queue notification threw");
            }
        }

        ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory;
        std::string id;
        mc_plugin_preferences_service preferences;
        std::string directory;
        runtime::IMessageQueue& queue;
        audio::PlaybackService& service;
        mc_context& context;
        plugin::PluginLibraryService libraryService;
        mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
        mc_plugin_playback_service playbackService = MC_PLUGIN_PLAYBACK_SERVICE_DEFAULT;
        mc_plugin_indexer_service indexerService = MC_PLUGIN_INDEXER_SERVICE_DEFAULT;
        mc_playback* playback = nullptr;
        void* instance = nullptr;
        bool started = false;
        bool registered = false;
        bool stopRequested = false;
        std::mutex commandMutex;
        std::deque<Command> commands;
        bool accepting = false;
    };

    PlaybackRemotes::PlaybackRemotes(mc_context& context, LibraryBackend::Shared local,
        audio::PlaybackService& playback, runtime::IMessageQueue& queue)
    : context(context)
    , local(std::move(local))
    , playback(playback)
    , queue(queue)
    , owner(std::this_thread::get_id())
    {
        const auto plugins = ListPlugins();
        this->remotes.reserve(plugins.size());
        try {
            for (const auto& info : plugins) {
                if (!info.enabled) {
                    continue;
                }
                auto factory = FindPlaybackRemoteFactory(info.id);
                if (!factory) {
                    continue;
                }
                try {
                    this->remotes.push_back(
                        std::make_unique<Remote>(context, this->local, playback, queue, std::move(factory), info.name,
                            info.id));
                }
                catch (const std::runtime_error& error) {
                    musik::debug::error("PlaybackRemotes", "could not start " + info.name + ": " + error.what());
                }
            }
        }
        catch (...) {
            /* earlier remotes may depend on one another during shutdown. */
            this->Stop();
            throw;
        }
    }

    PlaybackRemotes::~PlaybackRemotes() { this->Stop(); }

    mc_result PlaybackRemotes::ValidateCallingThread() const {
        return std::this_thread::get_id() == this->owner ? MC_OK : MC_ERROR_WRONG_THREAD;
    }

    mc_result PlaybackRemotes::Reload(std::string_view pluginId) {
        const auto thread = this->ValidateCallingThread();
        if (thread != MC_OK) {
            return thread;
        }
        const auto plugins = ListPlugins();
        const auto info = std::find_if(plugins.begin(), plugins.end(), [&](const auto& candidate) {
            return candidate.enabled && candidate.id == pluginId;
        });
        if (info == plugins.end()) {
            return MC_ERROR_UNSUPPORTED;
        }
        auto factory = FindPlaybackRemoteFactory(pluginId);
        if (!factory) {
            return MC_ERROR_UNSUPPORTED;
        }
        const auto existing = std::find_if(this->remotes.begin(), this->remotes.end(), [&](const auto& remote) {
            return remote->Id() == pluginId;
        });
        if (existing != this->remotes.end()) {
            /* a callback cannot reload the plugin whose code is still running. */
            musik::core::plugin::ValidateShutdown();
            /* workers can wait for accepted owner commands, so settle those
            before destroying the old server and starting its replacement. */
            (*existing)->Stop();
            this->remotes.erase(existing);
        }
        try {
            this->remotes.push_back(std::make_unique<Remote>(this->context, this->local,
                this->playback, this->queue, std::move(factory), info->name, info->id));
            return MC_OK;
        }
        catch (const std::runtime_error& error) {
            musik::debug::error("PlaybackRemotes", "could not restart " + info->name + ": " + error.what());
            return MC_ERROR_PLUGIN;
        }
    }

    void PlaybackRemotes::Stop() {
        /* all workers must stop accepting work before any worker is joined. */
        for (const auto& remote : this->remotes) {
            remote->RequestStop();
        }
        for (const auto& remote : this->remotes) {
            remote->CancelPending();
        }
        this->remotes.clear();
    }

} } }
