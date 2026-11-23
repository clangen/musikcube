#include <musikcore/ffi/consumer/player-handle.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/PlaybackWorkers.h>
#include <common/runtime/MessageQueue.h>
#include <common/runtime/Message.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <thread>

namespace musik { namespace core { namespace ffi {
    namespace {
        /* file URIs require decoding before the regular-file fallback. provider
        URIs keep their spelling; catalog credential URIs belong to track APIs. */
        mc_result normalizeUri(mc_string_view view, std::string& uri) {
            uri.assign(view.data, view.length);
            const auto colon = uri.find(':');
            std::string scheme = uri.substr(0, colon);
            std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            if (scheme == "musikcore") {
                return MC_ERROR_UNSUPPORTED;
            }
            if (scheme != "file" || colon == std::string::npos) {
                return MC_OK;
            }
            if (uri.substr(colon, 3) != "://") {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            const auto start = uri.find('/', colon + 3);
            if (start == std::string::npos ||
                (start != colon + 3 && uri.substr(colon + 3, start - colon - 3) != "localhost")) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            std::string decoded;
            for (size_t i = start; i < uri.size(); ++i) {
                unsigned char value = uri[i];
                if (value == '?' || value == '#') {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                if (value == '%') {
                    const auto hex = [](unsigned char c) -> int {
                        if (c >= '0' && c <= '9') {
                            return c - '0';
                        }
                        if (c >= 'a' && c <= 'f') {
                            return c - 'a' + 10;
                        }
                        if (c >= 'A' && c <= 'F') {
                            return c - 'A' + 10;
                        }
                        return -1;
                    };
                    if (i + 2 >= uri.size() || hex(uri[i + 1]) < 0 || hex(uri[i + 2]) < 0) {
                        return MC_ERROR_INVALID_ARGUMENT;
                    }
                    value = (unsigned char) (hex(uri[i + 1]) * 16 + hex(uri[i + 2]));
                    i += 2;
                    if (!value) {
                        return MC_ERROR_INVALID_ARGUMENT;
                    }
                }
                decoded.push_back((char) value);
            }
#ifdef WIN32
            if (decoded.size() > 2 && decoded[0] == '/' && decoded[2] == ':') {
                decoded.erase(0, 1);
            }
#endif
            uri = std::move(decoded);
            return MC_OK;
        }

        /* carries the observer generation so a detached dispatch batch cannot
        deliver an earlier notification to a replacement observer. */
        class ChangeMessage final : public runtime::Message {
            public:
                ChangeMessage(runtime::IMessageTarget* target, int type, uint64_t generation)
                : runtime::Message(target, type, 0, 0)
                , generation(generation) {}
                uint64_t Generation() const { return this->generation; }

            private:
                const uint64_t generation;
        };

        /* plays one URI through its own output. retired workers may finish
        returning buffers after replacement. only the selected track can
        update the snapshot delivered on the context thread. */
        class Session final : public UriPlayerSession, public runtime::IMessageTarget,
            private audio::Player::EventListener
        {
            public:
                static constexpr int Changed = 1;

                explicit Session(std::shared_ptr<runtime::MessageQueue> queue)
                : queue(std::move(queue))
                , owner(std::this_thread::get_id())
                , output(audio::outputs::SelectedOutput()) {
                    this->queue->Register(this);
                }
                ~Session() override { this->Shutdown(); }
                Session(const Session&) = delete;
                Session& operator=(const Session&) = delete;

                mc_result Check() const {
                    if (this->owner != std::this_thread::get_id()) {
                        return MC_ERROR_WRONG_THREAD;
                    }
                    return this->active.load() ? MC_OK : MC_ERROR_NOT_FOUND;
                }

                mc_result Close() {
                    if (this->owner != std::this_thread::get_id()) {
                        return MC_ERROR_WRONG_THREAD;
                    }
                    if (this->delivering) {
                        return MC_ERROR_BUSY;
                    }
                    this->Shutdown();
                    return MC_OK;
                }

                void Shutdown() override {
                    if (!this->active.exchange(false)) {
                        return;
                    }
                    this->callback = nullptr;
                    this->data = nullptr;
                    /* workers can still publish their final state while joining. */
                    this->workers.Close();
                    this->track.reset();
                    this->output.reset();
                    this->queue->Unregister(this);
                }

                mc_result Play(mc_string_view view) {
                    const auto status = this->Check();
                    if (status != MC_OK) {
                        return status;
                    }
                    std::string uri;
                    const auto normalized = normalizeUri(view, uri);
                    if (normalized != MC_OK) {
                        return normalized;
                    }
                    std::lock_guard lock(this->mutex);
                    this->StopTrack();
                    if (this->output->Resume() == MC_OK) {
                        /* publish the selection before a fast open can report failure. */
                        this->track = this->workers.Start(uri, this->output, audio::Player::Drain::No, {});
                        this->state = MC_PLAYER_BUFFERING;
                        this->track->Play();
                    }
                    else {
                        this->state = MC_PLAYER_FAILED;
                    }
                    this->Notify();
                    return MC_OK;
                }

                mc_result Control(PlayerCommand command, double value) {
                    const auto status = this->Check();
                    if (status != MC_OK) {
                        return status;
                    }
                    std::lock_guard lock(this->mutex);
                    switch (command) {
                        case PlayerCommand::Pause:
                        case PlayerCommand::Resume: {
                            if (!this->track) {
                                return MC_ERROR_NOT_FOUND;
                            }
                            const bool pause = command == PlayerCommand::Pause;
                            const auto result = pause ? this->output->Pause() : this->output->Resume();
                            if (result != MC_OK) {
                                return result;
                            }
                            this->state = pause ? MC_PLAYER_PAUSED : MC_PLAYER_PLAYING;
                        }
                        break;
                        case PlayerCommand::Stop:
                            this->StopTrack();
                            break;
                        case PlayerCommand::Seek:
                            if (!this->track) {
                                return MC_ERROR_NOT_FOUND;
                            }
                            if (this->track->GetStreamState() == sdk::StreamState::Buffering) {
                                return MC_ERROR_BUSY;
                            }
                            if (!this->track->SetPosition(value)) {
                                return MC_ERROR_NOT_FOUND;
                            }
                            this->state = MC_PLAYER_PLAYING;
                            break;
                        case PlayerCommand::Volume:
                            this->output->SetVolume(value);
                            break;
                    }
                    this->Notify();
                    return MC_OK;
                }

                mc_result Snapshot(mc_player_snapshot& result) const {
                    const auto status = this->Check();
                    if (status != MC_OK) {
                        return status;
                    }
                    std::lock_guard lock(this->mutex);
                    result.state = this->state;
                    result.position = this->track ? this->track->GetPosition() : 0.0;
                    result.duration = this->track ? this->track->GetDuration() : -1.0;
                    result.volume = this->output->GetVolume();
                    return MC_OK;
                }

                mc_result SetCallback(mc_player_changed_fn callback, void* data) {
                    const auto status = this->Check();
                    if (status != MC_OK) {
                        return status;
                    }
                    std::lock_guard lock(this->mutex);
                    ++this->generation;
                    this->queue->Remove(this);
                    this->pending = false;
                    this->callback = callback;
                    this->data = callback ? data : nullptr;
                    return MC_OK;
                }

                void ProcessMessage(runtime::IMessage& message) override {
                    {
                        std::lock_guard lock(this->mutex);
                        if (static_cast<const ChangeMessage&>(message).Generation() != this->generation) {
                            return;
                        }
                        this->pending = false;
                    }
                    if (this->active.load() && this->callback) {
                        mc_player_snapshot snapshot = MC_PLAYER_SNAPSHOT_DEFAULT;
                        this->Snapshot(snapshot);
                        this->delivering = true;
                        exception_barrier(nullptr, [&] {
                            this->callback(this->data, &snapshot);
                            return MC_OK;
                        });
                        this->delivering = false;
                    }
                }

            private:
                /* the session mutex protects selection and notification together.
                a replaced worker can finish, but it cannot publish over the new URI. */
                void StopTrack() {
                    if (this->track) {
                        this->track->RequestStop();
                        this->track.reset();
                    }
                    this->output->Stop();
                    this->state = MC_PLAYER_STOPPED;
                }

                void OnPlayerBuffered(audio::Player* sender) override {
                    std::lock_guard lock(this->mutex);
                    if (sender == this->track.get()) {
                        /* duration becomes available even if the listener paused during open. */
                        this->Notify();
                    }
                }

                void OnPlayerStarted(audio::Player* sender) override {
                    std::lock_guard lock(this->mutex);
                    if (sender == this->track.get() && this->state == MC_PLAYER_BUFFERING) {
                        this->state = MC_PLAYER_PLAYING;
                        this->Notify();
                    }
                }

                void OnPlayerFinished(audio::Player* sender) override {
                    this->Finish(sender, MC_PLAYER_FINISHED);
                }

                void OnPlayerOpenFailed(audio::Player* sender) override {
                    this->Finish(sender, MC_PLAYER_FAILED);
                }

                void Finish(audio::Player* sender, mc_player_state terminal) {
                    std::lock_guard lock(this->mutex);
                    if (sender == this->track.get()) {
                        this->track.reset();
                        this->state = terminal;
                        this->Notify();
                    }
                }

                void Notify() {
                    if (this->active.load() && !this->pending) {
                        /* decoder workers signal the queue; foreign callbacks never
                        run on those workers or on an audio output callback. */
                        this->queue->Post(std::make_shared<ChangeMessage>(this, Changed, this->generation));
                        this->pending = true;
                    }
                }

                const std::shared_ptr<runtime::MessageQueue> queue;
                const std::thread::id owner;
                std::shared_ptr<audio::Output> output;
                audio::Player::Shared track;
                audio::PlaybackWorkers workers { *this };
                mc_player_state state = MC_PLAYER_STOPPED;
                std::atomic<bool> active { true };
                mutable std::mutex mutex;
                uint64_t generation = 0;
                bool pending = false;
                mc_player_changed_fn callback = nullptr;
                void* data = nullptr;
                bool delivering = false;
        };

        /* owns one session reference. C destruction stops the resource; it is
        deliberately different from releasing a catalog playback handle. */
        class Handle final : public mc_player {
            public:
                explicit Handle(std::shared_ptr<Session> session)
                : session(std::move(session)) {}
                mc_result Play(mc_string_view uri) override { return this->session->Play(uri); }
                mc_result Control(PlayerCommand command, double value) override { return this->session->Control(command, value); }
                mc_result Snapshot(mc_player_snapshot& result) const override { return this->session->Snapshot(result); }
                mc_result SetCallback(mc_player_changed_fn callback, void* data) override { return this->session->SetCallback(callback, data); }
                mc_result Close() override { return this->session->Close(); }

            private:
                std::shared_ptr<Session> session;
        };
    }

    std::shared_ptr<UriPlayerSession> CreateUriPlayer(std::shared_ptr<runtime::MessageQueue> queue, mc_player** out) {
        auto session = std::make_shared<Session>(std::move(queue));
        std::unique_ptr<mc_player> handle(new Handle(session));
        *out = handle.release();
        return session;
    }
} } }
