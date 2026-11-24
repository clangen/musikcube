#pragma once

#include <musikcore/plugin/media.h>
#include <musikcore/plugin/encoder.h>
#include <memory>
#include <optional>
#include <string>

namespace server {

    /* a response owns its encoder until the last encoded byte is consumed. */
    struct ReleaseEncoder {
        void operator()(mc_encoder* encoder) const {
            if (encoder) {
                encoder->api->destroy(encoder->instance);
                delete encoder;
            }
        }
    };

    using EncoderOwner = std::unique_ptr<mc_encoder, ReleaseEncoder>;

    /* opens a URI through musikcore's stream providers. the server uses it to
    read track bytes or give a file encoder a writable destination. destroying
    it releases the provider; call Close explicitly to check for write errors
    before publishing a finished file. */
    class MediaStream {
    public:
        MediaStream() = default;
        MediaStream(const MediaStream&) = delete;
        ~MediaStream();
        MediaStream& operator=(const MediaStream&) = delete;

        bool Open(const mc_media_host_api& media, const std::string& uri, mc_stream_open_flags flags);
        bool Close();
        const mc_stream& Handle() const { return stream; }

    private:
        mc_stream stream = MC_STREAM_DEFAULT;
    };

    /* opens an input track and finds the codec that can decode it. both server
    transcoders use Read to feed one block of PCM to their encoder at a time.
    consume the samples before the next Read reuses the scratch buffer. */
    class DecodedAudio {
    public:
        DecodedAudio() = default;
        DecodedAudio(const DecodedAudio&) = delete;
        ~DecodedAudio();
        DecodedAudio& operator=(const DecodedAudio&) = delete;

        bool Open(const mc_media_host_api& media, const mc_audio_scratch_buffer_api& scratch, const std::string& uri);
        mc_result Read(mc_pcm_view& pcm);
        bool HasDuration() const;

    private:
        MediaStream input;
        mc_decoder decoder = MC_DECODER_DEFAULT;
        const mc_audio_scratch_buffer_api* scratch = nullptr;
        mc_audio_scratch_buffer* buffer = nullptr;
        mc_decode_state state = MC_DECODE_MORE;
    };

    /* supplies bytes to microhttpd for one response. a file can answer a seek
    immediately; a live transcode produces the next bytes as the client reads.
    the response owns the source until the transfer finishes or disconnects. */
    class ResponseBody {
    public:
        virtual ~ResponseBody() = default;
        virtual mc_result Read(uint64_t offset, char* dst, size_t capacity, size_t& count) = 0;
        virtual std::optional<uint64_t> Length() const = 0;
        virtual bool Seekable() const = 0;
    };

    /* on success the response takes ownership and clears the filename. */
    std::unique_ptr<ResponseBody> TakeTemporaryFileBody(const mc_media_host_api& media, std::string& filename);

    std::unique_ptr<ResponseBody> OpenFileBody(const mc_media_host_api& media, const std::string& filename);
}
