#include "legacy-library-selection.h"
#include "library-binding.h"
#include "playback-binding.h"
#include "request.h"
#include "../common/error.h"

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/library/MasterLibrary.h>
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

    ILibraryPtr CreateLegacyLibrary() {
        return std::make_shared<library::MasterLibrary>();
    }

    void AttachLegacyLibrary(mc_context& context, const ILibraryPtr& library, audio::PlaybackService& playback) {
        auto* adapter = static_cast<library::MasterLibrary*>(library.get());
        auto& queue = adapter->GetMessageQueue();
        const auto notifications = std::make_shared<CoreNotifications>(context);

        /* Detach the engine and context references before releasing the adapter. */
        std::shared_ptr<library::MasterLibrary> bound(adapter, [library, notifications, &queue](auto*) {
            auto& selected = static_cast<musik::core::library::MasterLibrary&>(*library);
            queue.UnregisterForBroadcasts(notifications.get());
            selected.AttachContext(nullptr, nullptr);
        });
        auto& factory = LibraryFactory::Instance();
        attach_library_selector(context,
            [bound](mc_library_kind kind) {
                const auto type = kind == MC_LIBRARY_LOCAL ? ILibrary::Type::Local : ILibrary::Type::Remote;
                bound->Select(type);
                Preferences::ForComponent(prefs::components::Settings)
                    ->SetInt(prefs::keys::LibraryType, static_cast<int>(type));
                return MC_OK;
            },
            [&factory] {
                const auto remote = std::static_pointer_cast<library::RemoteLibrary>(factory.DefaultRemoteLibrary());
                remote->Disconnect();
                remote->Connect();
            },
            *factory.DefaultLocalLibrary()->Indexer());
        adapter->AttachContext(&context, &playback);
        queue.RegisterForBroadcasts(notifications);
    }

    /* Owns the core queue and its readiness thread. Neither copyable nor movable. */
    class ManagedRuntime final : public ContextRuntime, public sigslot::has_slots<> {
    public:
        ~ManagedRuntime() override {
            this->Stop();
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

            auto engine = std::make_shared<audio::PlaybackService>(*this->queue, this->active);
            this->playback = engine.get();
            own_context_playback(context, engine);
            audio::AttachPlayback(context, *engine);

            this->notifications = std::make_shared<CoreNotifications>(context);
            this->queue->RegisterForBroadcasts(this->notifications);

            this->remote->ConnectionStateChanged.connect(this, &ManagedRuntime::OnRemoteStateChanged);
            this->indexer->Started.connect(this, &ManagedRuntime::OnIndexStarted);
            this->indexer->Progress.connect(this, &ManagedRuntime::OnIndexProgress);
            this->indexer->Finished.connect(this, &ManagedRuntime::OnIndexFinished);
            attach_library_selector(context,
                [this](mc_library_kind kind) { return this->SelectLibrary(kind); },
                [this] {
                    this->remote->Disconnect();
                    this->remote->Connect();
                },
                *this->indexer);
            this->PublishSelection(true);

            /* The plugin metadata proxy and indexer always serve the local catalog. */
            plugin::Start(this->queue.get(), engine.get(), this->local);

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

    private:
        mc_result SelectLibrary(mc_library_kind kind) {
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

        void PublishSelection(bool selectionChanged) {
            auto status = CaptureLibraryStatus(this->active);
            auto identity = status.state == MC_LIBRARY_CONNECTED ? CaptureLibrary(this->active) : nullptr;
            publish_library_selection(*this->context, std::move(identity), std::move(status), selectionChanged);
        }

        void OnRemoteStateChanged(ILibrary::ConnectionState) {
            if (this->active == this->remote) {
                audio::DiscardUnavailablePlaybackQueue(*this->playback);
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
        audio::PlaybackService* playback = nullptr;
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
