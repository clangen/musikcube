#include <musikcore/ffi/consumer/library-query-backends.h>
#include <musikcore/ffi/consumer/request.h>
#include <musikcore/ffi/common/error.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Messages.h>

#include <condition_variable>

namespace musik { namespace core { namespace ffi {
    /* Borrows the context for the lifetime of its runtime broadcasts. Neither copyable nor movable. */
    class CoreNotifications final : public runtime::IMessageTarget {
    public:
        explicit CoreNotifications(mc_context& context) : context(context) {}
        CoreNotifications(const CoreNotifications&) = delete;
        CoreNotifications(CoreNotifications&&) = delete;

        CoreNotifications& operator=(const CoreNotifications&) = delete;
        CoreNotifications& operator=(CoreNotifications&&) = delete;

        void ProcessMessage(runtime::IMessage& input) override {
            mc_playlist_event event;
            switch (input.Type()) {
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
    };

    /* Owns the core queue and its readiness thread. Neither copyable nor movable. */
    class ManagedRuntime final : public ContextRuntime, public sigslot::has_slots<> {
    public:
        ~ManagedRuntime() override {
            this->Stop();
            this->playback.reset();
            audio::Player::WaitForWorkers();
            audio::vis::Shutdown();
            LibraryFactory::Shutdown();
            this->queue->Close();
        }

        void Start(mc_context& context) override {
            this->context = &context;
            this->initialized = true;
            plugin::Init();
            LibraryFactory::Initialize(this->queue);

            auto& factory = LibraryFactory::Instance();
            this->local = factory.DefaultLocalLibrary();
            this->remote = std::static_pointer_cast<library::RemoteLibrary>(factory.DefaultRemoteLibrary());
            this->indexer = this->local->Indexer();

            const auto prefs = Preferences::ForComponent(prefs::components::Settings);
            const auto catalog = prefs->GetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
            this->active = catalog == static_cast<int>(ILibrary::Type::Remote) ? ILibraryPtr(this->remote) : this->local;

            this->playback = std::make_unique<audio::PlaybackService>(*this->queue, this->active);

            this->notifications = std::make_shared<CoreNotifications>(context);
            this->queue->RegisterForBroadcasts(this->notifications);

            this->remote->ConnectionStateChanged.connect(this, &ManagedRuntime::OnRemoteStateChanged);
            this->indexer->Started.connect(this, &ManagedRuntime::OnIndexStarted);
            this->indexer->Progress.connect(this, &ManagedRuntime::OnIndexProgress);
            this->indexer->Finished.connect(this, &ManagedRuntime::OnIndexFinished);
            this->PublishSelection(true);

            /* The plugin metadata proxy and indexer always serve the local catalog. */
            plugin::Start(this->queue.get(), this->playback.get(), this->local);

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

        void AcknowledgeDispatch() override {
            std::lock_guard lock(this->mutex);
            ++this->dispatches;
            this->changed.notify_all();
        }

        mc_result OpenPlayback(mc_playback** out) override {
            return this->playback->OpenHandle(out);
        }

        void ReconnectRemote() override {
            this->remote->Disconnect();
            this->remote->Connect();
        }

        IIndexer& LocalIndexer() const override {
            return *this->local->Indexer();
        }

        void Stop() override {
            {
                std::lock_guard lock(this->mutex);
                this->stopping.store(true);
                this->changed.notify_all();
            }
            this->queue->WakeWaiters();
            if (this->notifier.joinable()) {
                this->notifier.join();
            }

            if (this->indexer) {
                this->indexer->Shutdown();
                this->indexer->Started.disconnect(this);
                this->indexer->Progress.disconnect(this);
                this->indexer->Finished.disconnect(this);
                this->indexer = nullptr;
            }
            if (this->remote) {
                this->remote->ConnectionStateChanged.disconnect(this);
            }
            if (this->notifications) {
                this->queue->UnregisterForBroadcasts(this->notifications.get());
                this->notifications.reset();
            }
            if (this->initialized) {
                this->initialized = false;
                exception_barrier(nullptr, [] {
                    plugin::Shutdown();
                    return MC_OK;
                });
            }
        }

        mc_result SelectLibrary(mc_library_kind kind) override {
            const ILibraryPtr selected = kind == MC_LIBRARY_LOCAL ? this->local : this->remote;
            if (this->active != selected) {
                this->playback->SelectLibrary(selected);
                this->active = selected;
                this->PublishSelection(true);
            }

            Preferences::ForComponent(prefs::components::Settings)
                ->SetInt(prefs::keys::LibraryType, static_cast<int>(selected->GetType()));
            return MC_OK;
        }

    private:
        void PublishSelection(bool selectionChanged) {
            auto status = CaptureLibraryStatus(this->active);
            auto identity = status.state == MC_LIBRARY_CONNECTED ? CaptureLibrary(this->active) : nullptr;
            publish_library_selection(*this->context, std::move(identity), std::move(status), selectionChanged);
        }

        void OnRemoteStateChanged(ILibrary::ConnectionState) {
            if (this->active == this->remote) {
                this->playback->InvalidateDisconnectedCatalog();
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
        std::shared_ptr<CoreNotifications> notifications;
        mc_context* context = nullptr;
        std::unique_ptr<audio::PlaybackService> playback;
        std::mutex mutex;
        std::condition_variable changed;
        uint64_t dispatches = 0;
        IIndexer* indexer = nullptr;
        bool initialized = false;
        std::atomic<bool> stopping { false };
        std::thread notifier;
    };

    std::unique_ptr<ContextRuntime> make_context_runtime() {
        return std::make_unique<ManagedRuntime>();
    }
} } }
