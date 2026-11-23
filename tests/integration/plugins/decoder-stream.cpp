#include <common/FilesystemPath.h>
#include "../../support/plugins/PluginCandidates.h"
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/io/LocalFileSource.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

using musik::core::ffi::PluginRegistry;
namespace scratch = musik::core::ffi::audio_scratch_buffers;

/* keep failures visible even when the integration host is built in Release. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "decoder/stream: %s\n", message);
        std::abort();
    }
}

/* inspect scratch through the host's read-only view. the last partial block
and the empty EOF call catch lost tail frames and accidental sample replay. */
static void expectFrames(mc_decoder decoder, mc_audio_scratch_buffer* buffer, unsigned first, size_t count, mc_decode_state expected) {
    mc_decode_state state = MC_DECODE_MORE;
    require(decoder.api->decode(decoder.instance, buffer, &state, nullptr) == MC_OK, "decode");
    require(state == expected, "decode state");

    mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
    require(scratch::Read(buffer, &pcm) == MC_OK && pcm.frame_count == count &&
        pcm.format.channels == 1 && pcm.format.sample_rate == 8, "PCM format and frame count");
    for (size_t i = 0; i < count; ++i) {
        require(pcm.data[i] == static_cast<float>(first + i) / 256, "PCM sample");
    }
}

/* exercise one factory using only the interfaces available to an external C
decoder. release all instances before dropping their shared library. */
static void run(const mc_decoder_factory& factory, mc_stream& input) {
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    require(input.api->get_info(input.instance, &info) == MC_OK, "input metadata");
    mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
    uint32_t supported = 0;
    require(factory.can_handle(factory.context, &hint, &supported) == MC_OK && !supported, "unrecognized type");
    hint.extension = info.extension;
    require(factory.can_handle(factory.context, &hint, &supported) == MC_OK && supported, "probe opened source");

    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.audio_scratch = &scratch::Host();
    mc_decoder decoder = MC_DECODER_DEFAULT;
    decoder.api = factory.instance_api;
    require(factory.create(factory.context, &services, &decoder.instance, nullptr) == MC_OK, "create");
    require(decoder.api->open(decoder.instance, &input, nullptr) == MC_OK, "open");

    double seconds = 0;
    require(decoder.api->get_duration(decoder.instance, &seconds) == MC_OK && seconds == 0.75, "duration");
    require(decoder.api->set_preferred_sample_rate(decoder.instance, 96000) == MC_ERROR_UNSUPPORTED, "native rate");
    mc_audio_scratch_buffer* buffer = nullptr;
    const mc_audio_format format { sizeof(mc_audio_format), 8, 1, 0 };
    require(services.audio_scratch->create(nullptr, &format, 0, &buffer, nullptr) == MC_OK, "allocate scratch");
    expectFrames(decoder, buffer, 0, 4, MC_DECODE_MORE);
    expectFrames(decoder, buffer, 4, 2, MC_DECODE_END);
    expectFrames(decoder, buffer, 0, 0, MC_DECODE_END);

    /* seek after EOF must restart the same instance at its actual frame. */
    require(decoder.api->seek(decoder.instance, 0.30, &seconds, nullptr) == MC_OK && seconds == 0.25, "seek");
    expectFrames(decoder, buffer, 2, 4, MC_DECODE_END);

    decoder.api->interrupt(decoder.instance);
    mc_decode_state state = MC_DECODE_MORE;
    require(decoder.api->decode(decoder.instance, buffer, &state, nullptr) == MC_ERROR_CANCELLED, "cancelled decode");
    services.audio_scratch->destroy(nullptr, &buffer);
    decoder.api->destroy(decoder.instance);
}

#ifdef MUSIKCORE_TEST_HTTP_SOURCE
int checkProgressiveHttpRead(const char* pluginPath);
int checkHttpCacheClient(const char* pluginPath, const char* directory, const std::string& url, char expected);
#endif

int main(int argc, char** argv) {
#ifdef MUSIKCORE_TEST_HTTP_SOURCE
    if (argc == 6 && std::string_view(argv[1]) == "--cache-client") {
        return checkHttpCacheClient(argv[2], argv[3], argv[4], argv[5][0]);
    }
    if (argc == 3 && std::string_view(argv[1]) == "--progressive-http") {
        return checkProgressiveHttpRead(argv[2]);
    }
#endif
    require(argc == 2, "fixture DLL argument");
    const auto dll = musik::core::sdk::fs::pathFromUtf8(argv[1]);
    PluginCandidates candidates(dll.parent_path() / "decoder-runs");
    candidates.Add(dll, "decoder");
    PluginRegistry registry;
    require(registry.Initialize(candidates.Directory()) == MC_OK, "start registry");
    const auto factory = registry.Find<mc_decoder_factory>("test.bytes");
    require(static_cast<bool>(factory), "registered decoder");

    const auto filename = candidates.Directory("test.bytes");
    const std::array<char, 6> samples { 0, 1, 2, 3, 4, 5 };
    std::ofstream(musik::core::sdk::fs::pathFromUtf8(filename), std::ios::binary).write(samples.data(), samples.size());

    mc_stream input = MC_STREAM_DEFAULT;
    require(musik::core::io::CreateLocalFileSource(input) == MC_OK, "create input");
    const mc_string_view uri { filename.data(), filename.size() };
    require(input.api->open(input.instance, uri, MC_STREAM_OPEN_READ, nullptr) == MC_OK, "open input");
    require(factory.Invoke([&](const auto& table) {
        run(table, input);
        return MC_OK;
    }) == MC_OK, "use decoder");
    input.api->destroy(input.instance);
    require(registry.Shutdown() == MC_OK, "unload");
}
