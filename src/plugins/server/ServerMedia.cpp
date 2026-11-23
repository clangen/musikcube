#include "ServerMedia.h"
#include <cmath>
#include <filesystem>

namespace server {

    MediaStream::~MediaStream() {
        if (stream.instance) {
            stream.api->destroy(stream.instance);
        }
    }

    bool MediaStream::Open(Context& context, const std::string& uri, mc_stream_open_flags flags) {
        const auto& media = context.environment->Media();
        const mc_string_view path {
            .data = uri.data(),
            .length = uri.size(),
        };
        return media.create_stream(media.context, path, &stream, nullptr) == MC_OK &&
            stream.api->open(stream.instance, path, flags, nullptr) == MC_OK;
    }

    bool MediaStream::Close() {
        return stream.api->close(stream.instance) == MC_OK;
    }

    DecodedAudio::~DecodedAudio() {
        if (decoder.instance) {
            decoder.api->destroy(decoder.instance);
        }
        if (buffer) {
            scratch->destroy(scratch->context, &buffer);
        }
    }

    bool DecodedAudio::Open(Context& context, const std::string& uri) {
        if (!input.Open(context, uri, MC_STREAM_OPEN_READ)) {
            return false;
        }

        const auto& media = context.environment->Media();
        if (media.create_decoder(media.context, &input.Handle(), &decoder, nullptr) != MC_OK ||
            decoder.api->open(decoder.instance, &input.Handle(), nullptr) != MC_OK)
        {
            return false;
        }

        /* the codec supplies the actual format with its first block. */
        scratch = &context.environment->AudioScratch();
        const mc_audio_format initial {
            .struct_size = sizeof(mc_audio_format),
            .sample_rate = 44100,
            .channels = 2,
            .channel_layout = 0,
        };
        return scratch->create(scratch->context, &initial, 0, &buffer, nullptr) == MC_OK;
    }

    bool DecodedAudio::HasDuration() const {
        double seconds = 0;
        return decoder.api->get_duration(decoder.instance, &seconds) == MC_OK &&
            std::isfinite(seconds) && seconds >= 0;
    }

    mc_result DecodedAudio::Read(mc_pcm_view& pcm) {
        pcm = MC_PCM_VIEW_DEFAULT;
        if (state == MC_DECODE_END) {
            return MC_OK;
        }
        const auto status = decoder.api->decode(decoder.instance, buffer, &state, nullptr);
        if (status != MC_OK) {
            return status;
        }
        const auto read = scratch->read_block(scratch->context, buffer, &pcm);
        if (read != MC_OK) {
            return read;
        }
        if ((state != MC_DECODE_END && state != MC_DECODE_MORE) || (!pcm.frame_count && state != MC_DECODE_END)) {
            return MC_ERROR_PLUGIN;
        }
        return MC_OK;
    }

    class FileBody final : public ResponseBody {
    public:
        bool Open(Context& context, const std::string& filename) {
            if (!file.Open(context, filename, MC_STREAM_OPEN_READ)) {
                return false;
            }
            const auto& stream = file.Handle();
            mc_stream_info info = MC_STREAM_INFO_DEFAULT;
            if (stream.api->get_info(stream.instance, &info) != MC_OK) {
                return false;
            }
            if (info.flags & MC_STREAM_LENGTH_KNOWN) {
                length = info.length_bytes;
            }
            seekable = info.flags & MC_STREAM_SEEKABLE;
            return true;
        }

        void TakeTemporaryFile(std::string& filename) {
            temporary.path = std::filesystem::u8path(filename);
            filename.clear();
        }

        std::optional<uint64_t> Length() const override { return length; }
        bool Seekable() const override { return seekable; }

        mc_result Read(uint64_t offset, char* dst, size_t capacity, size_t& count) override {
            const auto& stream = file.Handle();
            count = 0;
            if (offset != position) {
                if (!seekable) {
                    return MC_ERROR_UNSUPPORTED;
                }
                const auto status = stream.api->seek(stream.instance, offset, nullptr);
                if (status != MC_OK) {
                    return status;
                }
                position = offset;
                ended = false;
            }
            if (ended) {
                return MC_OK;
            }
            mc_read_state state = MC_READ_MORE;
            const auto status = stream.api->read(stream.instance, dst, capacity, &count, &state, nullptr);
            if (status == MC_OK) {
                if (count > capacity || (!count && capacity && state != MC_READ_END)) {
                    return MC_ERROR_PLUGIN;
                }
                position += count;
                ended = state == MC_READ_END;
            }
            return status;
        }

    private:
        /* close the reader before deleting its file on Windows. */
        struct TemporaryFile {
            std::filesystem::path path;
            ~TemporaryFile() {
                if (!path.empty()) {
                    std::error_code error;
                    std::filesystem::remove(path, error);
                }
            }
        } temporary;
        MediaStream file;
        std::optional<uint64_t> length;
        uint64_t position = 0;
        bool seekable = false;
        bool ended = false;
    };

    static std::unique_ptr<FileBody> openFile(Context& context, const std::string& filename) {
        auto body = std::make_unique<FileBody>();
        if (!body->Open(context, filename)) {
            return {};
        }
        return body;
    }

    std::unique_ptr<ResponseBody> OpenFileBody(Context& context, const std::string& filename) {
        return openFile(context, filename);
    }

    std::unique_ptr<ResponseBody> TakeTemporaryFileBody(Context& context, std::string& filename) {
        auto body = openFile(context, filename);
        if (body) {
            body->TakeTemporaryFile(filename);
        }
        return body;
    }
}
