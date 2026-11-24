#include "../../support/plugins/PluginCandidates.h"
#include <common/FilesystemPath.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/plugin/encoder.h>

#include <cstdio>
#include <cstdlib>

using musik::core::ffi::PluginRegistry;

/* keep contract failures visible in release builds as well. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "encoder: %s\n", message);
        std::abort();
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "plugin filename");
    const auto library = musik::core::sdk::fs::pathFromUtf8(argv[1]);
    PluginCandidates candidates(library.parent_path() / "encoder-runs");
    candidates.Add(library, "encoder");

    PluginRegistry registry;
    require(registry.Initialize(candidates.Directory()) == MC_OK, "initialize registry");
    auto factory = registry.Find<mc_encoder_factory>("test.bytes.encoder");
    require(static_cast<bool>(factory), "find encoder factory");
    require(factory.Invoke([&](const mc_encoder_factory& api) -> mc_result {
        uint32_t supported = 1;
        require(api.can_handle(api.context, { ".other", 6 }, &supported) == MC_OK && !supported, "decline format");
        require(api.can_handle(api.context, { ".bytes", 6 }, &supported) == MC_OK && supported, "accept format");

        mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
        mc_encoder first = MC_ENCODER_DEFAULT;
        mc_encoder second = MC_ENCODER_DEFAULT;
        const mc_string_view format { ".bytes", 6 };
        require(api.create(api.context, &services, format, &first, nullptr) == MC_OK, "create first");
        require(api.create(api.context, &services, format, &second, nullptr) == MC_OK, "create second");
        require(first.instance && second.instance && first.instance != second.instance, "independent instances");
        require(first.api && first.api->streaming == 1, "streaming methods");
        require(first.api->initialize(first.instance, nullptr, 0, 1, 32) == MC_ERROR_INVALID_ARGUMENT,
            "reject invalid rate");
        require(first.api->initialize(first.instance, nullptr, 8, 1, 32) == MC_OK, "initialize first");
        require(second.api->initialize(second.instance, nullptr, 8, 1, 32) == MC_OK, "initialize second");

        const float samples[] { 0.1f, 0.2f };
        const mc_pcm_view pcm {
            .struct_size = sizeof(mc_pcm_view),
            .data = samples,
            .frame_count = 2,
            .format = {
                .struct_size = sizeof(mc_audio_format),
                .sample_rate = 8,
                .channels = 1,
                .channel_layout = 0,
            },
        };
        mc_encoded_block firstBlock = MC_ENCODED_BLOCK_DEFAULT;
        mc_encoded_block secondBlock = MC_ENCODED_BLOCK_DEFAULT;
        require(first.api->encode(first.instance, &pcm, &firstBlock) == MC_OK, "encode first");
        require(firstBlock.length == 2 && firstBlock.data[0] == 1 && firstBlock.data[1] == 2, "first bytes");
        require(second.api->encode(second.instance, &pcm, &secondBlock) == MC_OK, "encode second");
        require(firstBlock.data != secondBlock.data && firstBlock.data[0] == 1, "separate packet storage");
        require(first.api->flush(first.instance, &firstBlock) == MC_OK &&
            firstBlock.length == 1 && firstBlock.data[0] == '!', "flush tail");
        require(first.api->finalize(first.instance, { nullptr, 0 }) == MC_OK, "finalize first");
        first.api->destroy(first.instance);
        require(secondBlock.data[0] == 1, "second remains valid after first destroy");
        second.api->destroy(second.instance);
        return MC_OK;
    }) == MC_OK, "use factory");
    require(registry.Shutdown() == MC_OK, "unload after destruction");
}
