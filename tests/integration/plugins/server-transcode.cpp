#include <common/FilesystemPath.h>
#include "../../support/check.h"
#include <musikcore/musikcore.hpp>
#include <musikcore/plugin/Plugins.h>
#include <common/Filesystem.h>
#include <plugins/server/BlockingTranscoder.h>
#include <plugins/server/TranscodingAudioDataStream.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    /* select encode failure or emit two bytes per block and three at flush.
    the caller deliberately supplies a one-byte HTTP buffer. */
    struct TestEncoder {
        bool fail;
        char block[2] { 'a', 'b' };
        char tail[3] { 'x', 'y', 'z' };
    };

    const mc_encoder_vtable testEncoderApi {
        sizeof(mc_encoder_vtable),
        [](void* state) { delete static_cast<TestEncoder*>(state); },
        1,
        [](void*, const mc_stream*, uint32_t, uint32_t, uint32_t) -> mc_result { return MC_OK; },
        [](void* state, const mc_pcm_view*, mc_encoded_block* out) -> mc_result {
            auto& encoder = *static_cast<TestEncoder*>(state);
            out->data = encoder.block;
            out->length = 2;
            return encoder.fail ? MC_ERROR_PLUGIN : MC_OK;
        },
        [](void* state, mc_encoded_block* out) -> mc_result {
            out->data = static_cast<TestEncoder*>(state)->tail;
            out->length = 3;
            return MC_OK;
        },
        [](void*, mc_string_view) -> mc_result { return MC_OK; },
    };

    server::EncoderOwner testEncoder(bool fail) {
        auto encoder = std::make_unique<mc_encoder>(mc_encoder MC_ENCODER_DEFAULT);
        encoder->instance = new TestEncoder { fail };
        encoder->api = &testEncoderApi;
        return server::EncoderOwner(encoder.release());
    }

    const mc_encoder_vtable badTrailerApi {
        sizeof(mc_encoder_vtable),
        [](void*) {},
        0,
        [](void*, const mc_stream*, uint32_t, uint32_t, uint32_t) -> mc_result { return MC_OK; },
        [](void*, const mc_pcm_view*, mc_encoded_block*) -> mc_result { return MC_OK; },
        [](void*, mc_encoded_block*) -> mc_result { return MC_OK; },
        [](void*, mc_string_view) -> mc_result { return MC_ERROR_PLUGIN; },
    };
#ifdef TEST_SERVER_STOCK_ENCODER
    /* allow the real FLAC muxer to write its header, then simulate a full disk
    while it emits delayed packets and the trailer. */
    bool checkWriteFailure(Context& context) {
        server::MediaStream output;
        const auto path = musik::core::sdk::fs::pathFromUtf8(musik::core::api::ReadPath(MC_PATH_DATA)) / "failed.flac";
        if (!TEST_CHECK(output.Open(*context.media, musik::core::sdk::fs::pathToUtf8(path),
                MC_STREAM_OPEN_WRITE | MC_STREAM_OPEN_CREATE | MC_STREAM_OPEN_TRUNCATE)))
        {
            return false;
        }
        const auto& file = output.Handle();

        /* only writes change. seek and length still refer to the original file. */
        struct WriteGate {
            bool fail = false;
            const mc_stream_vtable* methods = nullptr;
        };
        static thread_local WriteGate gate;
        gate = {
            .fail = false,
            .methods = file.api,
        };
        auto methods = *file.api;
        methods.write = [](void* instance, const void* src, size_t size, size_t* count, mc_error** error) -> mc_result {
            if (gate.fail) {
                *count = 0;
                return MC_ERROR_IO;
            }
            return gate.methods->write(instance, src, size, count, error);
        };
        const mc_stream destination {
            .struct_size = sizeof(mc_stream),
            .instance = file.instance,
            .api = &methods,
        };
        const float silence[] { 0 };
        const mc_pcm_view pcm {
            .struct_size = sizeof(mc_pcm_view),
            .data = silence,
            .frame_count = 1,
            .format = {
                .struct_size = sizeof(mc_audio_format),
                .sample_rate = 8000,
                .channels = 1,
                .channel_layout = 0,
            },
        };
        const auto& media = *context.media;
        mc_encoder encoder = MC_ENCODER_DEFAULT;
        constexpr char format[] = ".flac";
        if (!TEST_CHECK(media.create_encoder(media.context, { format, sizeof(format) - 1 }, &encoder, nullptr) == MC_OK)) {
            return false;
        }
        mc_encoded_block block = MC_ENCODED_BLOCK_DEFAULT;
        const bool prepared = encoder.api->initialize(encoder.instance, &destination, 8000, 1, 128) == MC_OK &&
            encoder.api->encode(encoder.instance, &pcm, &block) == MC_OK;
        gate.fail = true;
        const bool failed = encoder.api->finalize(encoder.instance, { nullptr, 0 }) != MC_OK;
        encoder.api->destroy(encoder.instance);
        return TEST_CHECK(prepared && failed);
    }
#endif

}

namespace runtime_tests {
    /* drive the production request body with a real C decoder and host scratch.
    destruction after the first byte is the deterministic HTTP disconnect seam. */
    bool checkServerTranscode() {
        auto opts = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        opts.services = MC_SERVICE_PLUGINS;
        musik::core::api::Context runtime(opts);
        Context context;
        context.media = &musik::core::plugin::Media();
        context.scratch = &musik::core::plugin::AudioScratch();
        context.preferences = musik::core::plugin::ReadSettingsService("musikcube Server (wss, http)");
        context.dataDirectory = musik::core::api::ReadPath(MC_PATH_DATA);
        const auto root = musik::core::sdk::fs::pathFromUtf8(musik::core::api::ReadPath(MC_PATH_DATA));
        const auto path = [&](const char* name) {
            return musik::core::sdk::fs::pathToUtf8(root / name);
        };
        const auto source = path("fixture.bytes");
        const auto temp = path("audio.tmp");
        const auto cached = path("audio.encoded");
        std::ofstream(musik::core::sdk::fs::pathFromUtf8(source), std::ios::binary).write("123456", 6);
        const auto clean = [&] {
            return !fs::exists(musik::core::sdk::fs::pathFromUtf8(temp)) && !fs::exists(musik::core::sdk::fs::pathFromUtf8(cached));
        };

        /* a disconnect after returning a single encoded byte discards the file. */
        char byte;
        size_t count = 0;
        {
            TranscodingAudioDataStream body(testEncoder(false), 128);
            if (!TEST_CHECK(body.Open(context, source, false, temp, cached)) ||
                !TEST_CHECK(body.Read(0, &byte, 1, count) == MC_OK && count == 1 && byte == 'a') ||
                !TEST_CHECK(fs::exists(musik::core::sdk::fs::pathFromUtf8(temp)) && !fs::exists(musik::core::sdk::fs::pathFromUtf8(cached))))
            {
                return false;
            }
        }
        if (!TEST_CHECK(clean())) {
            return false;
        }

        /* flush bytes can span several reads. publish only the exact final file. */
        {
            TranscodingAudioDataStream body(testEncoder(false), 128);
            if (!TEST_CHECK(body.Open(context, source, false, temp, cached))) {
                return false;
            }
            std::string bytes;
            do {
                if (!TEST_CHECK(body.Read(bytes.size(), &byte, 1, count) == MC_OK)) {
                    return false;
                }
                bytes.append(&byte, count);
            } while (count && bytes.size() < 16);
            std::ifstream file(musik::core::sdk::fs::pathFromUtf8(cached), std::ios::binary);
            const std::string contents(std::istreambuf_iterator<char>(file), {});
            if (!TEST_CHECK(bytes == "ababxyz" && bytes == contents)) {
                return false;
            }
        }
        fs::remove(musik::core::sdk::fs::pathFromUtf8(cached));

        /* a source with no duration cannot produce a seekable file on demand.
        ordinary playback can still read its bytes without opening a cache. */
        const auto live = path("live.bytes");
        std::ofstream(musik::core::sdk::fs::pathFromUtf8(live), std::ios::binary).write("123456", 6);
        {
            TranscodingAudioDataStream body(testEncoder(false), 128);
            if (!TEST_CHECK(body.Open(context, live, false, temp, cached)) || !TEST_CHECK(!body.IsCaching() && !body.Finish(context)) ||
                !TEST_CHECK(body.Read(0, &byte, 1, count) == MC_OK && count == 1 && byte == 'a') ||
                !TEST_CHECK(clean()))
            {
                return false;
            }
        }

        /* with caching disabled, two responses to the same request own their
        encoded files independently. closing one must leave the other readable. */
        const auto secondTemp = path("audio-2.tmp");
        const auto uncached = [&](const std::string& filename) -> std::unique_ptr<server::ResponseBody> {
            TranscodingAudioDataStream body(testEncoder(false), 128);
            if (!body.Open(context, source, false, filename, {})) {
                return {};
            }
            return body.Finish(context);
        };
        auto first = uncached(temp);
        auto second = uncached(secondTemp);
        if (!TEST_CHECK(first && second && first->Length() == 7 && second->Length() == 7)) {
            return false;
        }
        first.reset();
        if (!TEST_CHECK(clean() && fs::exists(musik::core::sdk::fs::pathFromUtf8(secondTemp))) ||
            !TEST_CHECK(second->Read(6, &byte, 1, count) == MC_OK && count == 1 && byte == 'z'))
        {
            return false;
        }
        second.reset();
        if (!TEST_CHECK(!fs::exists(musik::core::sdk::fs::pathFromUtf8(secondTemp)))) {
            return false;
        }

        /* errors before either encoder finishes must release resources and
        leave the source usable by a later request. */
        {
            TranscodingAudioDataStream body(testEncoder(true), 128);
            if (!TEST_CHECK(body.Open(context, source, false, temp, cached)) ||
                !TEST_CHECK(body.Read(0, &byte, 1, count) == MC_ERROR_PLUGIN))
            {
                return false;
            }
        }
#ifdef TEST_SERVER_STOCK_ENCODER
        if (!checkWriteFailure(context)) {
            return false;
        }
#endif
        mc_encoder broken {
            .struct_size = sizeof(mc_encoder),
            .instance = &context,
            .api = &badTrailerApi,
        };
        return TEST_CHECK(clean()) &&
            TEST_CHECK(!BlockingTranscoder::Run(context, broken, source, temp, cached, 128)) && TEST_CHECK(clean()) &&
            TEST_CHECK(TranscodingAudioDataStream::GetActiveCount() == 0);
    }
}
