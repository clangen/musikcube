#include <musikcore/ffi/consumer/library-query-backends.h>
#include <musikcore/ffi/consumer/library-storage.h>
#include <musikcore/ffi/consumer/request.h>
#include <musikcore/ffi/consumer/player-handle.h>
#include <musikcore/ffi/common/error.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/plugin/PlaybackRemotes.h>
#include <common/runtime/MessageQueue.h>
#include <common/runtime/Message.h>
#include <musikcore/debug.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Messages.h>

#include <condition_variable>
#include <stdexcept>

namespace musik { namespace core { namespace ffi {
    /* Owns text until dispatcher delivery. Copyable and movable. */
    class LogRecord final : public runtime::Message {
    public:
        LogRecord(mc_log_level level, const std::string& tag, const std::string& text)
            : runtime::Message(nullptr, message::LogRecord, 0, 0), level(level), tag(tag), text(text) {}

        void Deliver(mc_log_fn callback, void* data) const {
            exception_barrier(nullptr, [&] {
                callback(data, this->level, { this->tag.data(), this->tag.size() },
                    { this->text.data(), this->text.size() });
                return MC_OK;
            });
        }

    private:
        mc_log_level level;
        std::string tag;
        std::string text;
    };

    /* Borrows a queue that outlives the logging thread. */
    class DispatchLogBackend final : public debug::IBackend {
    public:
        explicit DispatchLogBackend(runtime::MessageQueue& queue) : queue(queue) {}

        void verbose(const std::string& tag, const std::string& text) override {
            this->Post(MC_LOG_VERBOSE, tag, text);
        }

        void info(const std::string& tag, const std::string& text) override {
            this->Post(MC_LOG_INFO, tag, text);
        }

        void warning(const std::string& tag, const std::string& text) override {
            this->Post(MC_LOG_WARNING, tag, text);
        }

        void error(const std::string& tag, const std::string& text) override {
            this->Post(MC_LOG_ERROR, tag, text);
        }

    private:
        void Post(mc_log_level level, const std::string& tag, const std::string& text) {
            exception_barrier(nullptr, [&] {
                this->queue.Broadcast(std::make_shared<LogRecord>(level, tag, text));
                return MC_OK;
            });
        }

        runtime::MessageQueue& queue;
    };

    /* Borrows the context for the lifetime of its runtime broadcasts. Neither copyable nor movable. */
    class CoreNotifications final : public runtime::IMessageTarget {
    public:
        CoreNotifications(mc_context& context, mc_log_fn log, void* logData)
            : context(context), log(log), logData(logData) {}
        CoreNotifications(const CoreNotifications&) = delete;
        CoreNotifications(CoreNotifications&&) = delete;

        CoreNotifications& operator=(const CoreNotifications&) = delete;
        CoreNotifications& operator=(CoreNotifications&&) = delete;

        void ProcessMessage(runtime::IMessage& input) override {
            mc_playlist_event event;
            switch (input.Type()) {
                case message::LogRecord:
                    if (this->log) {
                        static_cast<const LogRecord&>(input).Deliver(this->log, this->logData);
                    }
                    return;
                case message::EqualizerUpdated:
                    publish_equalizer_event(this->context);
                    return;
                case message::PlaylistModified:
                    event = MC_PLAYLIST_CONTENTS_CHANGED;
                    break;
                case message::PlaylistCreated:
                    event = MC_PLAYLIST_CREATED;
                    break;
                case message::PlaylistRenamed:
                    event = MC_PLAYLIST_RENAMED;
                    break;
                case message::PlaylistDeleted:
                    event = MC_PLAYLIST_DELETED;
                    break;
                default:
                    return;
            }

            publish_playlist_event(this->context, event, input.UserData1());
        }

    private:
        mc_context& context;
        const mc_log_fn log;
        void* const logData;
    };

    /* Owns the core queue and its readiness thread. Neither copyable nor movable. */
    class ManagedRuntime final : public ContextRuntime, private runtime::IMessageTarget {
    public:
        ~ManagedRuntime() override {
            try {
                this->Stop();
            }
            catch (...) {
                /* explicit shutdown reports failure; destruction must not throw. */
            }
        }

        void Start(mc_context& context, const std::string& pluginDirectory,
            mc_services services, uint32_t flags, mc_log_fn log, void* logData) override
        {
            this->context = &context;
            this->queue->Register(this);
            this->notifications = std::make_shared<CoreNotifications>(context, log, logData);
            this->queue->RegisterForBroadcasts(this->notifications);
            if (flags & MC_CONTEXT_ENABLE_LOGGING) {
                this->StartLogging(log != nullptr);
            }

            /* provider discovery precedes indexer and decoder creation. */
            if (services & MC_SERVICE_PLUGINS) {
                /* only session initialization needs plugin rollback. discovery
                rejection must not initialize the default registry during cleanup. */
                this->pluginsInitialized = true;
                musik::core::plugin::Initialize(pluginDirectory);
            }
            if (services & MC_SERVICE_LIBRARY) {
                this->librariesInitialized = true;
                /* suppress implicit network startup before the factory constructs its
                remote, rather than selecting local after Connect already ran. */
                musik::core::LibraryFactory::Initialize(this->queue,
                    (services & MC_SERVICE_INDEXER) != 0, !(flags & MC_CONTEXT_START_LOCAL));
                auto& factory = musik::core::LibraryFactory::Instance();
                this->local = factory.DefaultLocalLibrary();
                this->remote = std::static_pointer_cast<library::RemoteLibrary>(factory.DefaultRemoteLibrary());
                this->indexer = this->local->Indexer();

                const auto prefs = Preferences::ForComponent(prefs::components::Settings);
                const auto catalog = prefs->GetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
                const bool selectRemote = !(flags & MC_CONTEXT_START_LOCAL) &&
                    catalog == static_cast<int>(ILibrary::Type::Remote);
                this->active = selectRemote ? ILibraryPtr(this->remote) : this->local;
                this->remoteStateChanged = this->remote->ConnectionStateChanged.Subscribe([this](auto state) {
                    this->OnRemoteStateChanged(state);
                });
            }
            if (services & MC_SERVICE_PLAYBACK) {
                this->playback = std::make_unique<audio::PlaybackService>(*this->queue, this->active);
            }
            if (this->indexer) {
                this->indexStarted = this->indexer->Started.Subscribe([this] { this->OnIndexStarted(); });
                this->indexProgress = this->indexer->Progress.Subscribe([this](int updates) { this->OnIndexProgress(updates); });
                this->indexFinished = this->indexer->Finished.Subscribe([this](int updates) { this->OnIndexFinished(updates); });
                this->indexCancelled = this->indexer->Cancelled.Subscribe([this](int updates) {
                    publish_indexer_event(*this->context, MC_INDEXER_CANCELLED, updates);
                });
            }
            if (this->active) {
                this->PublishSelection(true);
            }

            /* metadata and optional scanner bindings use the local catalog. */
            if (this->pluginsInitialized) {
                musik::core::plugin::BindServices(this->queue.get());
            }
            if (this->playback) {
                this->playback->Initialize(true);
            }
            if (this->indexer) {
                /* automatic scans must see fully constructed catalogs, provider
                bindings, and event subscriptions before their workers start. */
                std::static_pointer_cast<library::LocalLibrary>(this->local)->ResumePendingMigrationScan();
            }

            if (this->playback) {
                this->playbackRemotes = std::make_unique<plugin::PlaybackRemotes>(context, this->local,
                    *this->playback, *this->queue);
                musik::core::plugin::BindPlaybackRemotes(*this->playbackRemotes);
            }

            this->notifier = std::thread([this, &context] {
                std::unique_lock lock(this->mutex);
                while (!this->stopping.load()) {
                    const auto before = this->dispatches;

                    /* Only the host drains the queue. Wait for that dispatch
                       before signaling readiness again. */
                    lock.unlock();
                    if (!this->queue->WaitForReady(this->stopping)) {
                        return;
                    }
                    wake_context(context);
                    lock.lock();
                    this->changed.wait(lock, [&] {
                        return this->stopping.load() || this->dispatches != before;
                    });
                }
            });
        }

        void Dispatch() override {
            this->queue->Dispatch();
        }

        void ProcessMessage(runtime::IMessage&) override {
            if (this->explicitLibrary && !this->explicitInvalid &&
                !this->explicitLibrary->playbackOrigin->Matches(this->active.get()))
            {
                /* A reconnect cannot rebind the selected handle to its new generation. */
                this->explicitInvalid = true;
                if (this->playback) {
                    this->playback->InvalidateDisconnectedCatalog();
                }
                auto status = CaptureLibraryStatus(this->active);
                status.state = MC_LIBRARY_DISCONNECTED;
                publish_library_selection(*this->context, nullptr, std::move(status), false);
            }
        }

        void AcknowledgeDispatch() override {
            std::lock_guard lock(this->mutex);
            ++this->dispatches;
            this->changed.notify_all();
        }

        mc_result CreatePlayer(mc_player** out) override {
            if (!this->pluginsInitialized) {
                return MC_ERROR_UNSUPPORTED;
            }
            /* allocate the registry slot before publishing an owned handle. */
            this->players.erase(std::remove_if(this->players.begin(), this->players.end(),
                [](const auto& player) { return player.expired(); }), this->players.end());
            this->players.emplace_back();
            this->players.back() = CreateUriPlayer(this->queue, out);
            return MC_OK;
        }

        mc_result OpenPlayback(mc_playback** out) override {
            return this->playback ? this->playback->OpenHandle(out) : MC_ERROR_UNSUPPORTED;
        }

        mc_result ReconnectRemote() override {
            if (!this->remote || this->explicitLibrary) {
                return MC_ERROR_UNSUPPORTED;
            }
            this->remote->Disconnect();
            this->remote->Connect();
            return MC_OK;
        }

        IIndexer* LocalIndexer() const override {
            return this->indexer;
        }

        void Stop() override {
            musik::core::plugin::ValidateShutdown();
            {
                std::lock_guard lock(this->mutex);
                if (this->stopping.exchange(true)) {
                    return;
                }
                this->changed.notify_all();
            }
            this->queue->WakeWaiters();
            if (this->notifier.joinable()) {
                this->notifier.join();
            }

            bool failure = false;
            musik::core::plugin::UnbindPlaybackRemotes();
            this->playbackRemotes.reset();
            for (auto& weak : this->players) {
                if (const auto player = weak.lock()) {
                    Cleanup([&] { player->Shutdown(); }, failure);
                }
            }
            this->players.clear();
            if (this->indexer) {
                Cleanup([&] { this->indexer->Shutdown(); }, failure);
                this->indexStarted.Reset();
                this->indexProgress.Reset();
                this->indexFinished.Reset();
                this->indexCancelled.Reset();
                this->indexer = nullptr;
            }
            this->remoteStateChanged.Reset();
            this->explicitStateChanged.Reset();
            this->queue->Unregister(this);
            if (this->notifications) {
                this->queue->UnregisterForBroadcasts(this->notifications.get());
                this->notifications.reset();
            }
            /* stop players while provider services and catalogs remain available. */
            if (this->playback) {
                Cleanup([&] { this->playback->Shutdown(); }, failure);
            }

            if (this->pluginsInitialized) {
                /* visualizer APIs can acquire providers without Playback. retire
                their cache for every provider session, after any player workers. */
                Cleanup([] { musik::core::audio::vis::Shutdown(); }, failure);
                Cleanup([] { musik::core::plugin::UnbindServices(); }, failure);
                /* transports own C output instances; release them while their
                plugin tables are still mapped, after server users have stopped. */
                Cleanup([&] { this->playback.reset(); }, failure);
                this->pluginsInitialized = false;
                Cleanup([] { musik::core::plugin::Shutdown(); }, failure);
            }
            Cleanup([&] { this->playback.reset(); }, failure);
            if (this->librariesInitialized) {
                this->librariesInitialized = false;
                Cleanup([] { musik::core::LibraryFactory::Shutdown(); }, failure);
            }
            Cleanup([&] { this->queue->Close(); }, failure);

            if (this->ownsLogging) {
                this->ownsLogging = false;
                Cleanup([] { debug::Shutdown(); }, failure);
            }
            if (failure) {
                throw std::runtime_error("Managed runtime shutdown failed");
            }
        }

        mc_result SelectLibrary(mc_library_kind kind) override {
            if (!this->local) {
                return MC_ERROR_UNSUPPORTED;
            }
            const ILibraryPtr selected = kind == MC_LIBRARY_LOCAL ? this->local : this->remote;
            const bool changed = this->active != selected || this->explicitLibrary;
            this->explicitStateChanged.Reset();
            this->explicitLibrary.reset();
            if (changed) {
                this->SwitchCatalog(selected);
                this->PublishSelection(true);
            }

            Preferences::ForComponent(prefs::components::Settings)
                ->SetInt(prefs::keys::LibraryType, static_cast<int>(selected->GetType()));

            /* a local-only startup has never connected this remote. selection is
            the explicit opt-in, and must not restart an already pending connection. */
            if (kind == MC_LIBRARY_REMOTE &&
                this->remote->WebSocketClient().ConnectionState() == library::RemoteLibrary::Client::State::Disconnected)
            {
                this->remote->Connect();
            }
            return MC_OK;
        }

        mc_result SelectLibrary(const std::shared_ptr<const LibraryIdentity>& library) override {
            const auto& origin = library->playbackOrigin;
            if (!origin) {
                return MC_ERROR_UNSUPPORTED;
            }
            auto selected = origin->Backend();
            if (this->explicitLibrary == library && !this->explicitInvalid) {
                return origin->Matches(selected.get()) ? MC_OK : MC_ERROR_NOT_FOUND;
            }

            /* Explicit sessions have their own message pump. The host dispatcher
               owns playback invalidation and library selection callbacks. */
            auto observer = selected->ConnectionStateChanged.Subscribe([this](auto) {
                this->queue->Post(runtime::Message::Create(this, ExplicitLibraryChanged));
            });
            if (!origin->Matches(selected.get())) {
                return MC_ERROR_NOT_FOUND;
            }
            this->SwitchCatalog(std::move(selected));
            this->explicitLibrary = library;
            this->explicitInvalid = false;
            this->explicitStateChanged = std::move(observer);
            return publish_library_selection(*this->context, library, CaptureLibraryStatus(this->active), true);
        }

    private:
        static constexpr int ExplicitLibraryChanged = 1;

        void SwitchCatalog(ILibraryPtr selected) {
            if (this->playback) {
                this->playback->InvalidateDisconnectedCatalog();
                this->playback->SelectLibrary(selected);
            }
            this->active = std::move(selected);
        }

        /* record failures without retaining provider-defined exception objects. */
        template<typename Work>
        static void Cleanup(Work&& work, bool& failure) noexcept {
            try {
                work();
            }
            catch (...) {
                failure = true;
            }
        }

        void StartLogging(bool observe) {
            std::vector<std::unique_ptr<debug::IBackend>> backends;
            backends.push_back(std::make_unique<debug::SimpleFileBackend>());
            if (observe) {
                backends.push_back(std::make_unique<DispatchLogBackend>(*this->queue));
            }

            this->ownsLogging = debug::TryStart(std::move(backends));
        }

        void PublishSelection(bool selectionChanged) {
            auto status = CaptureLibraryStatus(this->active);
            auto identity = status.state == MC_LIBRARY_CONNECTED ? CaptureLibrary(this->active) : nullptr;
            publish_library_selection(*this->context, std::move(identity), std::move(status), selectionChanged);
        }

        void OnRemoteStateChanged(ILibrary::ConnectionState) {
            if (!this->explicitLibrary && this->active == this->remote) {
                if (this->playback) {
                    this->playback->InvalidateDisconnectedCatalog();
                }
                this->PublishSelection(false);
            }
        }

        void OnIndexStarted() {
            publish_indexer_event(*this->context, MC_INDEXER_STARTED, 0);
        }

        void OnIndexProgress(int updates) {
            publish_indexer_event(*this->context, MC_INDEXER_PROGRESS, updates);
        }

        void OnIndexFinished(int updates) {
            publish_indexer_event(*this->context, MC_INDEXER_FINISHED, updates);
        }

        const std::shared_ptr<runtime::MessageQueue> queue = std::make_shared<runtime::MessageQueue>();
        ILibraryPtr local;
        std::shared_ptr<library::RemoteLibrary> remote;
        ILibraryPtr active;
        std::shared_ptr<const LibraryIdentity> explicitLibrary;
        bool explicitInvalid = false;
        std::shared_ptr<CoreNotifications> notifications;
        mc_context* context = nullptr;
        std::unique_ptr<audio::PlaybackService> playback;
        std::unique_ptr<plugin::PlaybackRemotes> playbackRemotes;
        std::vector<std::weak_ptr<UriPlayerSession>> players;
        std::mutex mutex;
        std::condition_variable changed;
        uint64_t dispatches = 0;
        IIndexer* indexer = nullptr;
        api::Subscriptions<ILibrary::ConnectionState>::Connection remoteStateChanged;
        api::Subscriptions<ILibrary::ConnectionState>::Connection explicitStateChanged;
        api::Subscriptions<>::Connection indexStarted;
        api::Subscriptions<int>::Connection indexProgress;
        api::Subscriptions<int>::Connection indexFinished;
        api::Subscriptions<int>::Connection indexCancelled;
        bool pluginsInitialized = false;
        bool librariesInitialized = false;
        bool ownsLogging = false;
        std::atomic<bool> stopping { false };
        std::thread notifier;
    };

    std::unique_ptr<ContextRuntime> make_context_runtime() {
        return std::make_unique<ManagedRuntime>();
    }
} } }
