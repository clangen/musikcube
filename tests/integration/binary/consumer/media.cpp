#include <musikcore/media.hpp>
#include <cstring>
#include <type_traits>
#include <utility>

using namespace musik::core::api;
static_assert(!std::is_copy_constructible<MediaStream>::value, "stream ownership");
static_assert(!std::is_copy_constructible<MediaDecoder>::value, "decoder ownership");
static_assert(!std::is_copy_constructible<MediaEncoder>::value, "encoder ownership");
static_assert(!std::is_copy_constructible<MediaPcm>::value, "PCM ownership");

extern "C" int media_cpp_contract(const char* directory, const char* inputPath, const char* outputPath) {
    try {
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.plugin_directory = { directory, std::strlen(directory) };
        Context context(options);
        MediaStream input(context, inputPath);
        if (input.Open() != MC_OK) {
            return 0;
        }
        MediaDecoder decoder(std::move(input));
        if (input.Get() || decoder.Open() != MC_OK) {
            return 0;
        }
        mc_media_read_state state = MC_MEDIA_MORE;
        auto pcm = decoder.Read(state);
        auto retained = std::move(pcm);
        if (pcm || !retained || retained.View().frame_count != 4) {
            return 0;
        }
        MediaStream output(context, outputPath, MC_MEDIA_OPEN_WRITE | MC_MEDIA_OPEN_CREATE | MC_MEDIA_OPEN_TRUNCATE);
        if (output.Open() != MC_OK) {
            return 0;
        }
        mc_media_encoder_options encoding = MC_MEDIA_ENCODER_OPTIONS_DEFAULT;
        encoding.format = retained.View().format;
        encoding.bitrate_kbps = 32;
        MediaEncoder encoder(std::move(output), ".bytes", encoding);
        auto moved = std::move(encoder);
        if (output.Get() || encoder.Get() || moved.Open() != MC_OK || moved.Write(retained.View()) != MC_OK ||
            moved.Finish() != MC_OK)
        {
            return 0;
        }
        if (context.Shutdown() != MC_OK || retained.View().data[0] != 0.25f) {
            return 0;
        }
        return 1;
    }
    catch (...) {
        return 0;
    }
}
