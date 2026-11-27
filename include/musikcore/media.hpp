#ifndef MUSIKCORE_MEDIA_HPP
#define MUSIKCORE_MEDIA_HPP

#include <musikcore/context.hpp>
#include <musikcore/media.h>
#include <exception>
#include <string_view>
#include <utility>

namespace musik { namespace core { namespace api {
    /* owns decoded samples independently of the decoder and context. noncopyable;
    moves transfer ownership and empty the source. views borrow from this object. */
    class MediaPcm {
    public:
        MediaPcm() noexcept = default;
        MediaPcm(const MediaPcm& other) = delete;
        MediaPcm(MediaPcm&& other) noexcept
        : pcm(std::exchange(other.pcm, nullptr)) {}
        ~MediaPcm() { mc_media_pcm_destroy(&this->pcm); }

        MediaPcm& operator=(const MediaPcm& other) = delete;
        MediaPcm& operator=(MediaPcm&& other) noexcept {
            if (this != &other) {
                mc_media_pcm_destroy(&this->pcm);
                this->pcm = std::exchange(other.pcm, nullptr);
            }
            return *this;
        }

        mc_media_pcm_view View() const {
            mc_media_pcm_view result = MC_MEDIA_PCM_VIEW_DEFAULT;
            const auto status = mc_media_pcm_get_view(this->pcm, &result);
            if (status != MC_OK) {
                throw Error(status);
            }
            return result;
        }
        explicit operator bool() const noexcept { return this->pcm != nullptr; }

    private:
        friend class MediaDecoder;
        explicit MediaPcm(mc_media_pcm* owned) noexcept
        : pcm(owned) {}

        mc_media_pcm* pcm = nullptr;
    };

    /* owns one byte stream. noncopyable; moves empty the source. creation does
    not open: publish the object before blocking in `Open()` so another thread
    can call `Interrupt()`. destruction requires callers to have stopped using it. */
    class MediaStream {
    public:
        MediaStream() noexcept = default;
        MediaStream(Context& context, std::string_view uri, mc_media_open_flags flags = MC_MEDIA_OPEN_READ) {
            const auto status =
                mc_media_stream_create(context.Get(), { uri.data(), uri.size() }, flags, &this->stream, nullptr);
            if (status != MC_OK) {
                throw Error(status);
            }
        }
        MediaStream(const MediaStream& other) = delete;
        MediaStream(MediaStream&& other) noexcept
        : stream(std::exchange(other.stream, nullptr)) {}
        ~MediaStream() {
            mc_media_stream_destroy(&this->stream);
            if (this->stream) {
                std::terminate();
            }
        }

        MediaStream& operator=(const MediaStream& other) = delete;
        MediaStream& operator=(MediaStream&& other) {
            if (this != &other) {
                const auto status = this->Close();
                if (status != MC_OK) {
                    throw Error(status);
                }
                this->stream = std::exchange(other.stream, nullptr);
            }
            return *this;
        }

        mc_result Open(mc_error** error = nullptr) { return mc_media_stream_open(this->stream, error); }
        mc_result Interrupt() { return mc_media_stream_interrupt(this->stream); }
        mc_result Read(
            void* data, size_t capacity, size_t& count, mc_media_read_state& state, mc_error** error = nullptr) {
            return mc_media_stream_read(this->stream, data, capacity, &count, &state, error);
        }
        mc_result Write(const void* data, size_t size, size_t& count, mc_error** error = nullptr) {
            return mc_media_stream_write(this->stream, data, size, &count, error);
        }
        mc_result Seek(uint64_t byteOffset, mc_error** error = nullptr) {
            return mc_media_stream_seek(this->stream, byteOffset, error);
        }
        mc_result Tell(uint64_t& byteOffset) const { return mc_media_stream_tell(this->stream, &byteOffset); }
        mc_media_stream_info Info() const {
            mc_media_stream_info result = MC_MEDIA_STREAM_INFO_DEFAULT;
            const auto status = mc_media_stream_get_info(this->stream, &result);
            if (status != MC_OK) {
                throw Error(status);
            }
            return result;
        }
        mc_result Close() { return mc_media_stream_destroy(&this->stream); }
        mc_media_stream* Get() noexcept { return this->stream; }
        const mc_media_stream* Get() const noexcept { return this->stream; }

    private:
        friend class MediaDecoder;
        friend class MediaEncoder;
        mc_media_stream* stream = nullptr;
    };

    /* owns a decoder and its input. noncopyable; moves empty the source. the
    constructor consumes an opened stream only on success; `Open()` may block.
    decoded PCM owns its samples. synchronize destruction with worker use. */
    class MediaDecoder {
    public:
        MediaDecoder() noexcept = default;
        explicit MediaDecoder(MediaStream&& input) {
            const auto status = mc_media_decoder_create(&input.stream, &this->decoder, nullptr);
            if (status != MC_OK) {
                throw Error(status);
            }
        }
        MediaDecoder(const MediaDecoder& other) = delete;
        MediaDecoder(MediaDecoder&& other) noexcept
        : decoder(std::exchange(other.decoder, nullptr)) {}
        ~MediaDecoder() {
            mc_media_decoder_destroy(&this->decoder);
            if (this->decoder) {
                std::terminate();
            }
        }

        MediaDecoder& operator=(const MediaDecoder& other) = delete;
        MediaDecoder& operator=(MediaDecoder&& other) {
            if (this != &other) {
                const auto status = this->Close();
                if (status != MC_OK) {
                    throw Error(status);
                }
                this->decoder = std::exchange(other.decoder, nullptr);
            }
            return *this;
        }

        mc_result Open(mc_error** error = nullptr) { return mc_media_decoder_open(this->decoder, error); }
        mc_result Interrupt() { return mc_media_decoder_interrupt(this->decoder); }
        MediaPcm Read(mc_media_read_state& state) {
            mc_media_pcm* result = nullptr;
            const auto status = mc_media_decoder_read(this->decoder, &result, &state, nullptr);
            if (status != MC_OK) {
                throw Error(status);
            }
            return MediaPcm(result);
        }
        mc_result Seek(double seconds, double& actual, mc_error** error = nullptr) {
            return mc_media_decoder_seek(this->decoder, seconds, &actual, error);
        }
        mc_result Duration(double& seconds) const { return mc_media_decoder_get_duration(this->decoder, &seconds); }
        mc_result Close() { return mc_media_decoder_destroy(&this->decoder); }
        mc_media_decoder* Get() noexcept { return this->decoder; }
        const mc_media_decoder* Get() const noexcept { return this->decoder; }

    private:
        mc_media_decoder* decoder = nullptr;
    };

    /* owns an encoder and its output. noncopyable; moves empty the source.
    construction consumes an opened output only on success. call `Finish()` to
    report delayed write/finalization failures; destruction alone aborts encoding.
    synchronize destruction with worker use. */
    class MediaEncoder {
    public:
        MediaEncoder() noexcept = default;
        MediaEncoder(MediaStream&& output, std::string_view format, const mc_media_encoder_options& options) {
            const auto status = mc_media_encoder_create(
                &output.stream, { format.data(), format.size() }, &options, &this->encoder, nullptr);
            if (status != MC_OK) {
                throw Error(status);
            }
        }
        MediaEncoder(const MediaEncoder& other) = delete;
        MediaEncoder(MediaEncoder&& other) noexcept
        : encoder(std::exchange(other.encoder, nullptr)) {}
        ~MediaEncoder() {
            mc_media_encoder_destroy(&this->encoder);
            if (this->encoder) {
                std::terminate();
            }
        }

        MediaEncoder& operator=(const MediaEncoder& other) = delete;
        MediaEncoder& operator=(MediaEncoder&& other) {
            if (this != &other) {
                const auto status = this->Close();
                if (status != MC_OK) {
                    throw Error(status);
                }
                this->encoder = std::exchange(other.encoder, nullptr);
            }
            return *this;
        }

        mc_result Open(mc_error** error = nullptr) { return mc_media_encoder_open(this->encoder, error); }
        mc_result Interrupt() { return mc_media_encoder_interrupt(this->encoder); }
        mc_result Write(const mc_media_pcm_view& pcm, mc_error** error = nullptr) {
            return mc_media_encoder_write(this->encoder, &pcm, error);
        }
        mc_result Finish(mc_error** error = nullptr) { return mc_media_encoder_finish(this->encoder, error); }
        mc_result Close() { return mc_media_encoder_destroy(&this->encoder); }
        mc_media_encoder* Get() noexcept { return this->encoder; }
        const mc_media_encoder* Get() const noexcept { return this->encoder; }

    private:
        mc_media_encoder* encoder = nullptr;
    };
} } }
#endif
