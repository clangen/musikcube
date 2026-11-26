#include "media-session.h"
#include "../common/error.h"
#include "../common/primitives.h"
#include <type_traits>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace musik { namespace core { namespace ffi {
    namespace {
        /* these callbacks cross a C boundary. continuing after a foreign
        exception could free storage still used by a provider's workers. */
        template<class F> auto invoke(F&& call) noexcept -> decltype(call()) {
            try {
                return call();
            }
            catch (...) {
                std::fputs("musikcore: media provider threw through its C ABI\n", stderr);
                std::abort();
            }
        }

        void requireReleased(bool released) noexcept {
            if (!released) {
                std::fputs("musikcore: application media resource escaped native cleanup\n", stderr);
                std::abort();
            }
        }

        bool text(mc_string_view value) {
            return valid_text(value, false);
        }

        bool formatValid(const mc_audio_format& format) {
            return format.struct_size >= sizeof(format) && format.sample_rate && format.channels;
        }

        mc_string_view view(const std::string& value) {
            return { value.data(), value.size() };
        }

        /* a root owns either a stream or a codec and its consumed stream. the
        session retains roots until native cleanup finishes, even if the caller
        releases its last public handle while shutdown is starting. */
        class Resource {
        public:
            virtual ~Resource() = default;
            virtual void Interrupt() = 0;
            virtual mc_result Close() = 0;

            std::mutex operation;
            std::mutex lifetime;
            std::atomic_bool cancelled { false };
            bool attempted = false;
            bool opened = false;
        };

        class Stream final : public Resource {
        public:
            ~Stream() override { requireReleased(!this->native.instance); }

            void Interrupt() override {
                std::lock_guard<std::mutex> lock(this->lifetime);
                this->cancelled = true;
                if (this->native.instance) {
                    invoke([&] {
                        this->native.api->interrupt(this->native.instance);
                    });
                }
            }

            mc_result Close() override {
                mc_result result = MC_OK;
                if (this->native.instance) {
                    result = invoke([&] {
                        return this->native.api->close(this->native.instance);
                    });
                    std::lock_guard<std::mutex> lock(this->lifetime);
                    invoke([&] {
                        this->native.api->destroy(this->native.instance);
                    });
                    this->native = MC_STREAM_DEFAULT;
                }
                this->opened = false;
                return result;
            }

            mc_stream native = MC_STREAM_DEFAULT;
            mc_media_open_flags flags = 0;
            std::string uri;
            std::string mime;
            std::string extension;
        };

        class Decoder final : public Resource {
        public:
            ~Decoder() override { requireReleased(!this->native.instance && !this->buffer); }

            void Interrupt() override {
                std::lock_guard<std::mutex> lock(this->lifetime);
                this->cancelled = true;
                if (this->native.instance) {
                    invoke([&] {
                        this->native.api->interrupt(this->native.instance);
                    });
                }
                this->input->Interrupt();
            }

            mc_result Close() override {
                {
                    std::lock_guard<std::mutex> lock(this->lifetime);
                    if (this->native.instance) {
                        invoke([&] {
                            this->native.api->destroy(this->native.instance);
                        });
                        this->native = MC_DECODER_DEFAULT;
                    }
                }
                if (this->buffer) {
                    invoke([&] {
                        this->scratch.destroy(this->scratch.context, &this->buffer);
                    });
                }
                this->opened = false;
                return this->input ? this->input->Close() : MC_OK;
            }

            mc_decoder native = MC_DECODER_DEFAULT;
            std::shared_ptr<Stream> input;
            mc_plugin_audio_scratch_service scratch = MC_PLUGIN_AUDIO_SCRATCH_SERVICE_DEFAULT;
            mc_audio_scratch_buffer* buffer = nullptr;
            bool ended = false;
        };

        class Encoder final : public Resource {
        public:
            ~Encoder() override { requireReleased(!this->native.instance); }

            void Interrupt() override {
                this->cancelled = true;
                this->output->Interrupt();
            }

            mc_result Close() override {
                if (this->native.instance) {
                    invoke([&] {
                        this->native.api->destroy(this->native.instance);
                    });
                    this->native = MC_ENCODER_DEFAULT;
                }
                this->opened = false;
                return this->output ? this->output->Close() : MC_OK;
            }

            mc_result WriteBlock(const mc_encoded_block& block) {
                if (block.length && !block.data) {
                    return MC_ERROR_PLUGIN;
                }
                size_t offset = 0;
                while (offset < block.length) {
                    if (this->cancelled) {
                        return MC_ERROR_CANCELLED;
                    }
                    size_t count = 0;
                    auto result = invoke([&] {
                        return this->output->native.api->write(
                            this->output->native.instance, block.data + offset, block.length - offset, &count, nullptr);
                    });
                    if (count > block.length - offset || (result == MC_OK && !count)) {
                        return MC_ERROR_PLUGIN;
                    }
                    if (result != MC_OK) {
                        return result;
                    }
                    offset += count;
                }
                return MC_OK;
            }

            mc_encoder native = MC_ENCODER_DEFAULT;
            std::shared_ptr<Stream> output;
            mc_media_encoder_options options = MC_MEDIA_ENCODER_OPTIONS_DEFAULT;
            bool finished = false;
            mc_result result = MC_OK;
        };
    }

    struct MediaSessionState {
        std::mutex mutex;
        std::condition_variable changed;
        bool closing = false;
        bool stopped = false;
        size_t active = 0;
        std::vector<std::shared_ptr<Resource>> roots;
        mc_plugin_media_service media;
        mc_plugin_audio_scratch_service scratch;
    };

    namespace {
        /* shutdown marks admission closed before interrupting roots. every
        admitted operation finishes cleanup before its guard releases admission. */
        class Call {
        public:
            explicit Call(const std::shared_ptr<MediaSessionState>& state)
            : state(state) {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->closing) {
                    ++state->active;
                    this->admitted = true;
                }
            }
            ~Call() {
                if (this->admitted) {
                    std::lock_guard<std::mutex> lock(this->state->mutex);
                    --this->state->active;
                    this->state->changed.notify_all();
                }
            }
            bool admitted = false;

        private:
            std::shared_ptr<MediaSessionState> state;
        };

        void removeRoot(MediaSessionState& state, const std::shared_ptr<Resource>& resource) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.roots.erase(std::remove(state.roots.begin(), state.roots.end(), resource), state.roots.end());
        }

        template<class Handle, class F>
        mc_result operate(const Handle* handle, mc_error** error, F&& body, bool allowCancelled = false) {
            if (!handle || (error && *error)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return exception_barrier(error, [&]() -> mc_result {
                Call call(handle->session);
                if (!call.admitted) {
                    return MC_ERROR_NOT_FOUND;
                }
                std::unique_lock<std::mutex> lock(handle->resource->operation, std::try_to_lock);
                if (!lock.owns_lock()) {
                    return MC_ERROR_BUSY;
                }
                const auto result = !allowCancelled && handle->resource->cancelled
                    ? MC_ERROR_CANCELLED
                    : body(*handle->resource);
                if (result != MC_OK && error && !*error) {
                    return fail(result, "media operation failed", error);
                }
                return result;
            });
        }

        template<class Handle> mc_result interrupt(Handle* handle) noexcept {
            if (!handle) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            Call call(handle->session);
            if (!call.admitted) {
                return MC_ERROR_NOT_FOUND;
            }
            handle->resource->Interrupt();
            return MC_OK;
        }

        template<class Handle> mc_result destroy(Handle** slot) noexcept {
            if (!slot || !*slot) {
                return MC_OK;
            }
            Handle* handle = *slot;
            mc_result result = MC_OK;
            {
                Call call(handle->session);
                if (!call.admitted) {
                    std::lock_guard<std::mutex> lock(handle->session->mutex);
                    if (!handle->session->stopped) {
                        return MC_ERROR_BUSY;
                    }
                }
                else {
                    std::unique_lock<std::mutex> lock(handle->resource->operation, std::try_to_lock);
                    if (!lock.owns_lock()) {
                        return MC_ERROR_BUSY;
                    }
                    result = handle->resource->Close();
                    removeRoot(*handle->session, handle->resource);
                }
            }
            delete handle;
            *slot = nullptr;
            return result;
        }

        template<class Handle, class Codec, class F>
        mc_result consume(mc_media_stream** input, Handle** output, mc_error** error, F&& create);
    }

    MediaSession::MediaSession(const mc_plugin_media_service& media, const mc_plugin_audio_scratch_service& scratch)
    : state(std::make_shared<MediaSessionState>()) {
        this->state->media = media;
        this->state->scratch = scratch;
    }

    MediaSession::~MediaSession() {
        this->Shutdown();
    }

    void MediaSession::Shutdown() noexcept {
        std::vector<std::shared_ptr<Resource>> roots;
        {
            std::unique_lock<std::mutex> lock(this->state->mutex);
            if (this->state->closing) {
                this->state->changed.wait(lock, [&] {
                    return this->state->stopped;
                });
                return;
            }
            this->state->closing = true;
            roots.swap(this->state->roots);
        }
        for (const auto& root : roots) {
            root->Interrupt();
        }
        {
            std::unique_lock<std::mutex> lock(this->state->mutex);
            this->state->changed.wait(lock, [&] {
                return !this->state->active;
            });
        }
        for (const auto& root : roots) {
            root->Close();
        }
        {
            std::lock_guard<std::mutex> lock(this->state->mutex);
            this->state->stopped = true;
            this->state->changed.notify_all();
        }
    }
} } }

using namespace musik::core::ffi;

struct mc_media_stream {
    std::shared_ptr<MediaSessionState> session;
    std::shared_ptr<Stream> resource;
};
struct mc_media_decoder {
    std::shared_ptr<MediaSessionState> session;
    std::shared_ptr<Decoder> resource;
};
struct mc_media_encoder {
    std::shared_ptr<MediaSessionState> session;
    std::shared_ptr<Encoder> resource;
};
struct mc_media_pcm {
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    std::vector<float> samples;
};

mc_result MediaSession::CreateStream(
    mc_string_view uri, mc_media_open_flags flags, mc_media_stream** output, mc_error** error)
{
    Call call(this->state);
    if (!call.admitted) {
        return MC_ERROR_NOT_FOUND;
    }
    auto resource = std::make_shared<Stream>();
    resource->uri.assign(uri.data, uri.length);
    resource->flags = flags;
    auto handle = std::make_unique<mc_media_stream>(mc_media_stream { this->state, resource });
    {
        std::lock_guard<std::mutex> lock(this->state->mutex);
        if (this->state->closing) {
            return MC_ERROR_CANCELLED;
        }
        this->state->roots.push_back(resource);
    }
    auto result = invoke([&] {
        std::lock_guard<std::mutex> lock(resource->lifetime);
        auto code = this->state->media.create_stream(this->state->media.context, uri, &resource->native, error);
        if (code == MC_OK && resource->cancelled) {
            resource->native.api->interrupt(resource->native.instance);
        }
        return code;
    });
    if (result != MC_OK) {
        resource->Close();
        removeRoot(*this->state, resource);
        return result;
    }
    *output = handle.release();
    return MC_OK;
}

namespace musik { namespace core { namespace ffi { namespace {
    template<class Handle, class Codec, class F>
    mc_result consume(mc_media_stream** input, Handle** output, mc_error** error, F&& create) {
        if (!input || !*input || !output || *output || (error && *error)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return operate(*input, error, [&](Stream& stream) -> mc_result {
            if (!stream.opened) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            const auto session = (*input)->session;
            auto codec = std::make_shared<Codec>();
            auto handle = std::make_unique<Handle>(Handle { session, codec });
            auto result = create(*codec, stream, *session);
            if (result != MC_OK) {
                codec->Close();
                return result;
            }
            /* replace the root atomically with respect to shutdown. if
            shutdown won, the original stream stays owned by its caller. */
            {
                std::unique_lock<std::mutex> lock(session->mutex);
                if (session->closing) {
                    lock.unlock();
                    codec->Close();
                    return MC_ERROR_CANCELLED;
                }
                const auto root = std::find(session->roots.begin(), session->roots.end(), (*input)->resource);
                if (root == session->roots.end()) {
                    std::fputs("musikcore: missing application media root\n", stderr);
                    std::abort();
                }
                if constexpr (std::is_same<Codec, Decoder>::value) {
                    codec->input = (*input)->resource;
                }
                else {
                    codec->output = (*input)->resource;
                }
                *root = codec;
            }
            delete *input;
            *input = nullptr;
            *output = handle.release();
            return MC_OK;
        });
    }
} } } }

mc_result MC_CALL mc_media_stream_create(
    mc_context* context, mc_string_view uri, mc_media_open_flags flags, mc_media_stream** output, mc_error** error)
{
    constexpr uint32_t allFlags =
        MC_MEDIA_OPEN_READ | MC_MEDIA_OPEN_WRITE | MC_MEDIA_OPEN_CREATE | MC_MEDIA_OPEN_TRUNCATE | MC_MEDIA_OPEN_APPEND;
    if (!context || !text(uri) || !output || *output || (error && *error) || (flags & ~allFlags) ||
        !(flags & (MC_MEDIA_OPEN_READ | MC_MEDIA_OPEN_WRITE)) ||
        (!(flags & MC_MEDIA_OPEN_WRITE) &&
            (flags & (MC_MEDIA_OPEN_CREATE | MC_MEDIA_OPEN_TRUNCATE | MC_MEDIA_OPEN_APPEND))) ||
        ((flags & MC_MEDIA_OPEN_TRUNCATE) && (flags & MC_MEDIA_OPEN_APPEND)))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(error, [&] {
        return create_media_stream(context, uri, flags, output, error);
    });
}

mc_result MC_CALL mc_media_stream_open(mc_media_stream* stream, mc_error** error) {
    return operate(stream, error, [&](Stream& resource) -> mc_result {
        if (resource.attempted) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        resource.attempted = true;
        auto result = invoke([&] {
            return resource.native.api->open(resource.native.instance, view(resource.uri), resource.flags, error);
        });
        if (result != MC_OK) {
            return result;
        }
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        result = invoke([&] {
            return resource.native.api->get_info(resource.native.instance, &info);
        });
        if (result != MC_OK) {
            return result;
        }
        auto copy = [](mc_string_view from, std::string& to) {
            if (from.length && !from.data) {
                return false;
            }
            to.assign(from.data ? from.data : "", from.length);
            return true;
        };
        if (!copy(info.content_type, resource.mime) || !copy(info.extension, resource.extension)) {
            return MC_ERROR_PLUGIN;
        }
        resource.opened = true;
        return MC_OK;
    });
}

mc_result MC_CALL mc_media_stream_interrupt(mc_media_stream* stream) {
    return interrupt(stream);
}
mc_result MC_CALL mc_media_stream_destroy(mc_media_stream** stream) {
    return destroy(stream);
}

mc_result MC_CALL mc_media_stream_get_info(const mc_media_stream* stream, mc_media_stream_info* info) {
    if (!info || info->struct_size < sizeof(*info)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(stream, nullptr, [&](Stream& resource) -> mc_result {
        if (!resource.opened) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        mc_stream_info current = MC_STREAM_INFO_DEFAULT;
        const auto result = invoke([&] {
            return resource.native.api->get_info(resource.native.instance, &current);
        });
        if (result != MC_OK) {
            return result;
        }
        *info = { sizeof(*info), current.flags, current.length_bytes, view(resource.uri), view(resource.mime),
            view(resource.extension) };
        return MC_OK;
    });
}

mc_result MC_CALL mc_media_stream_read(
    mc_media_stream* stream, void* data, size_t capacity, size_t* count, mc_media_read_state* state, mc_error** error)
{
    if ((!data && capacity) || !count || !state) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *count = 0;
    *state = MC_MEDIA_MORE;
    return operate(stream, error, [&](Stream& resource) -> mc_result {
        if (!resource.opened || !(resource.flags & MC_MEDIA_OPEN_READ)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto result = invoke([&] {
            return resource.native.api->read(resource.native.instance, data, capacity, count, state, error);
        });
        if (*count > capacity || *state > MC_MEDIA_END ||
            (result == MC_OK && capacity && !*count && *state == MC_MEDIA_MORE))
        {
            return MC_ERROR_PLUGIN;
        }
        return result;
    });
}

mc_result MC_CALL mc_media_stream_write(
    mc_media_stream* stream, const void* data, size_t size, size_t* count, mc_error** error)
{
    if ((!data && size) || !count) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *count = 0;
    return operate(stream, error, [&](Stream& resource) -> mc_result {
        if (!resource.opened || !(resource.flags & MC_MEDIA_OPEN_WRITE)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto result = invoke([&] {
            return resource.native.api->write(resource.native.instance, data, size, count, error);
        });
        return *count > size || (result == MC_OK && size && !*count) ? MC_ERROR_PLUGIN : result;
    });
}

mc_result MC_CALL mc_media_stream_seek(mc_media_stream* stream, uint64_t offset, mc_error** error) {
    return operate(stream, error, [&](Stream& resource) {
        return resource.opened ? invoke([&] {
            return resource.native.api->seek(resource.native.instance, offset, error);
        })
                               : MC_ERROR_INVALID_ARGUMENT;
    });
}

mc_result MC_CALL mc_media_stream_tell(const mc_media_stream* stream, uint64_t* offset) {
    if (!offset) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(stream, nullptr, [&](Stream& resource) {
        return resource.opened ? invoke([&] {
            return resource.native.api->tell(resource.native.instance, offset);
        })
                               : MC_ERROR_INVALID_ARGUMENT;
    });
}

mc_result MC_CALL mc_media_decoder_create(mc_media_stream** input, mc_media_decoder** output, mc_error** error) {
    return consume<mc_media_decoder, Decoder>(
        input, output, error, [&](Decoder& decoder, Stream& stream, MediaSessionState& session) -> mc_result {
            if (!(stream.flags & MC_MEDIA_OPEN_READ)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            decoder.scratch = session.scratch;
            return invoke([&] {
                return session.media.create_decoder(session.media.context, &stream.native, &decoder.native, error);
            });
        });
}

mc_result MC_CALL mc_media_decoder_open(mc_media_decoder* decoder, mc_error** error) {
    return operate(decoder, error, [&](Decoder& resource) -> mc_result {
        if (resource.attempted) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        resource.attempted = true;
        auto result = invoke([&] {
            return resource.native.api->open(resource.native.instance, &resource.input->native, error);
        });
        if (result != MC_OK) {
            return result;
        }
        mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
        format.sample_rate = 44100;
        format.channels = 2;
        result = invoke([&] {
            return resource.scratch.create(resource.scratch.context, &format, 0, &resource.buffer, error);
        });
        resource.opened = result == MC_OK;
        return result;
    });
}

mc_result MC_CALL mc_media_decoder_interrupt(mc_media_decoder* decoder) {
    return interrupt(decoder);
}
mc_result MC_CALL mc_media_decoder_destroy(mc_media_decoder** decoder) {
    return destroy(decoder);
}

mc_result MC_CALL mc_media_decoder_read(
    mc_media_decoder* decoder, mc_media_pcm** output, mc_media_read_state* state, mc_error** error)
{
    if (!output || *output || !state) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(decoder, error, [&](Decoder& resource) -> mc_result {
        if (!resource.opened) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (resource.ended) {
            *state = MC_MEDIA_END;
            return MC_OK;
        }
        auto pcm = std::make_unique<mc_media_pcm>();
        mc_decode_state decoded = MC_DECODE_MORE;
        auto result = invoke([&] {
            return resource.native.api->decode(resource.native.instance, resource.buffer, &decoded, error);
        });
        if (result != MC_OK) {
            return result;
        }
        mc_pcm_view block = MC_PCM_VIEW_DEFAULT;
        result = invoke([&] {
            return resource.scratch.read_block(resource.scratch.context, resource.buffer, &block);
        });
        if (result != MC_OK) {
            return result;
        }
        if (decoded > MC_DECODE_END || (!block.frame_count && decoded == MC_DECODE_MORE) ||
            (block.frame_count &&
                (!block.data || !formatValid(block.format) ||
                    block.frame_count > std::numeric_limits<size_t>::max() / sizeof(float) / block.format.channels)))
        {
            return MC_ERROR_PLUGIN;
        }
        if (block.frame_count) {
            pcm->format = block.format;
            pcm->samples.assign(block.data, block.data + block.frame_count * block.format.channels);
            *output = pcm.release();
        }
        resource.ended = decoded == MC_DECODE_END;
        *state = decoded;
        return MC_OK;
    });
}

mc_result MC_CALL mc_media_decoder_seek(mc_media_decoder* decoder, double seconds, double* actual, mc_error** error) {
    if (!actual || !std::isfinite(seconds) || seconds < 0) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(decoder, error, [&](Decoder& resource) -> mc_result {
        if (!resource.opened) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto result = invoke([&] {
            return resource.native.api->seek(resource.native.instance, seconds, actual, error);
        });
        if (result == MC_OK) {
            if (!std::isfinite(*actual) || *actual < 0) {
                return MC_ERROR_PLUGIN;
            }
            resource.ended = false;
        }
        return result;
    });
}

mc_result MC_CALL mc_media_decoder_get_duration(const mc_media_decoder* decoder, double* seconds) {
    if (!seconds) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(decoder, nullptr, [&](Decoder& resource) {
        return resource.opened ? invoke([&] {
            return resource.native.api->get_duration(resource.native.instance, seconds);
        })
                               : MC_ERROR_INVALID_ARGUMENT;
    });
}

mc_result MC_CALL mc_media_pcm_get_view(const mc_media_pcm* pcm, mc_media_pcm_view* output) {
    if (!pcm || !output || output->struct_size < sizeof(*output)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *output = { sizeof(*output), pcm->samples.data(), pcm->samples.size() / pcm->format.channels, pcm->format };
    return MC_OK;
}

void MC_CALL mc_media_pcm_destroy(mc_media_pcm** pcm) {
    if (pcm) {
        delete *pcm;
        *pcm = nullptr;
    }
}

mc_result MC_CALL mc_media_encoder_create(mc_media_stream** output, mc_string_view format,
    const mc_media_encoder_options* options, mc_media_encoder** encoder, mc_error** error)
{
    if (!text(format) || !options || options->struct_size < sizeof(*options) ||
        !formatValid(options->format) || !options->bitrate_kbps)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return consume<mc_media_encoder, Encoder>(
        output, encoder, error, [&](Encoder& resource, Stream& stream, MediaSessionState& session) -> mc_result {
            if (!(stream.flags & MC_MEDIA_OPEN_WRITE)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            resource.options = *options;
            return invoke([&] {
                return session.media.create_encoder(session.media.context, format, &resource.native, error);
            });
        });
}

mc_result MC_CALL mc_media_encoder_open(mc_media_encoder* encoder, mc_error** error) {
    return operate(encoder, error, [&](Encoder& resource) -> mc_result {
        if (resource.attempted || resource.finished) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        resource.attempted = true;
        resource.result = invoke([&] {
            return resource.native.api->initialize(resource.native.instance,
                resource.native.api->streaming ? nullptr : &resource.output->native,
                resource.options.format.sample_rate, resource.options.format.channels, resource.options.bitrate_kbps);
        });
        resource.opened = resource.result == MC_OK;
        return resource.result;
    });
}

mc_result MC_CALL mc_media_encoder_interrupt(mc_media_encoder* encoder) {
    return interrupt(encoder);
}
mc_result MC_CALL mc_media_encoder_destroy(mc_media_encoder** encoder) {
    return destroy(encoder);
}

mc_result MC_CALL mc_media_encoder_write(mc_media_encoder* encoder, const mc_media_pcm_view* pcm, mc_error** error) {
    if (!pcm || pcm->struct_size < sizeof(*pcm) || !formatValid(pcm->format) || (pcm->frame_count && !pcm->data) ||
        pcm->frame_count > std::numeric_limits<size_t>::max() / sizeof(float) / pcm->format.channels)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return operate(encoder, error, [&](Encoder& resource) -> mc_result {
        const auto& format = resource.options.format;
        if (!resource.opened || resource.finished || format.sample_rate != pcm->format.sample_rate ||
            format.channels != pcm->format.channels || format.channel_layout != pcm->format.channel_layout)
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (resource.result != MC_OK) {
            return resource.result;
        }
        mc_pcm_view block { sizeof(block), pcm->data, pcm->frame_count, pcm->format };
        mc_encoded_block encoded = MC_ENCODED_BLOCK_DEFAULT;
        resource.result = invoke([&] {
            return resource.native.api->encode(resource.native.instance, &block, &encoded);
        });
        if (resource.result == MC_OK) {
            resource.result = resource.WriteBlock(encoded);
        }
        return resource.result;
    });
}

mc_result MC_CALL mc_media_encoder_finish(mc_media_encoder* encoder, mc_error** error) {
    return operate(
        encoder, error,
        [&](Encoder& resource) -> mc_result {
            if (resource.finished) {
                return resource.result;
            }
            if (!resource.attempted) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (resource.cancelled && resource.result == MC_OK) {
                resource.result = MC_ERROR_CANCELLED;
            }
            if (resource.opened && resource.result == MC_OK) {
                mc_encoded_block encoded = MC_ENCODED_BLOCK_DEFAULT;
                resource.result = invoke([&] {
                    return resource.native.api->flush(resource.native.instance, &encoded);
                });
                if (resource.result == MC_OK) {
                    resource.result = resource.WriteBlock(encoded);
                }
                if (resource.result == MC_OK) {
                    resource.result = invoke([&] {
                        return resource.native.api->finalize(resource.native.instance, view(resource.output->uri));
                    });
                }
            }
            auto closeResult = resource.Close();
            if (resource.result == MC_OK) {
                resource.result = closeResult;
            }
            resource.finished = true;
            return resource.result;
        },
        true);
}
