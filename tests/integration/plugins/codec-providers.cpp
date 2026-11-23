#include <filesystem>
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/io/LocalFileSource.h>
#include <musikcore/plugin/media.h>
#include <musikcore/plugin/settings.h>

#include <cmath>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;
namespace scratch = musik::core::ffi::audio_scratch_buffers;
using musik::core::ffi::PluginRegistry;

namespace {
    /* failures remain visible in release builds. each scenario owns a fresh
    registry and temporary directory so copies cannot share codec state. */
    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "codec provider: %s\n", what);
            std::abort();
        }
    }

    /* a two-track NES sound file needs only a returning init/play routine to
    produce silence. track one proves the virtual URI survives the nested open. */
    std::vector<char> silentNsf() {
        std::vector<char> bytes(129);
        std::memcpy(bytes.data(), "NESM\x1a", 5);
        bytes[5] = 1;
        bytes[6] = 2;
        bytes[7] = 1;
        for (size_t highByte : { 9, 11, 13 }) {
            bytes[highByte] = static_cast<char>(0x80);
        }
        bytes[110] = 0x1a;
        bytes[111] = 0x41;
        bytes.back() = 0x60;
        return bytes;
    }

    /* an empty four-channel ProTracker pattern is a valid module with one
    subsong. the negative case asks for a second one that does not exist. */
    std::vector<char> silentMod() {
        std::vector<char> bytes(1084 + 64 * 4 * 4);
        bytes[950] = 1;
        std::memcpy(bytes.data() + 1080, "M.K.", 4);
        return bytes;
    }

    /* 1200 stereo frames force a short second GME block after the usual 1024.
    the legacy conversion wrote the full block even for a shorter destination. */
    mc_result MC_CALL readNumber(void*, mc_string_view key, double fallback, double* result) {
        *result = std::string_view(key.data, key.length) == "default_track_length_secs" ? 0.025 : fallback;
        return MC_OK;
    }

    /* one second of mono PCM at a quarter of full scale. write every word
    little-endian, including the RIFF lengths, independent of host byte order. */
    void writeWave(const fs::path& path, bool streaming = false) {
        constexpr unsigned rate = 32000;
        std::ofstream out(path, std::ios::binary);
        const auto word = [&](uint32_t value, size_t bytes) {
            for (size_t i = 0; i < bytes; ++i) {
                out.put(static_cast<char>(value >> (i * 8)));
            }
        };
        out.write("RIFF", 4);
        word(streaming ? UINT32_MAX : 36 + 2 * rate, 4);
        out.write("WAVEfmt ", 8);
        word(16, 4);
        word(1, 2);
        word(1, 2);
        word(rate, 4);
        word(2 * rate, 4);
        word(2, 2);
        word(16, 2);
        out.write("data", 4);
        word(streaming ? UINT32_MAX : 2 * rate, 4);
        for (size_t i = 0; i < rate; ++i) {
            word(8192, 2);
        }
    }

    /* observe the actual destination granted to FFmpeg by host scratch. the
    fixture decodes on one worker, keeping the real host context unchanged. */
    struct ScratchWrites {
        size_t calls = 0;
        float* samples = nullptr;
    };
    thread_local ScratchWrites writes;

    mc_result MC_CALL observeWrite(void* context, mc_audio_scratch_buffer* buffer, mc_audio_buffer_view* view) {
        const auto status = scratch::Host().begin_write(context, buffer, view);
        if (status == MC_OK) {
            ++writes.calls;
            writes.samples = view->data;
        }
        return status;
    }

    /* decoding at a different device rate must preserve the last samples
    delayed inside libswresample, including when EOF and a partial block coincide. */
    void checkResampler(
        const mc_decoder_factory& factory, mc_plugin_services services, const std::string& path)
    {
        auto observed = scratch::Host();
        observed.begin_write = observeWrite;
        services.audio_scratch = &observed;
        mc_stream input = MC_STREAM_DEFAULT;
        check(musik::core::io::CreateLocalFileSource(input) == MC_OK, "create file source");
        check(input.api->open(input.instance, { path.data(), path.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open PCM file");
        void* decoder = nullptr;
        const auto& api = *factory.instance_api;
        check(factory.create(factory.context, &services, &decoder, nullptr) == MC_OK, "create FFmpeg decoder");
        check(api.open(decoder, &input, nullptr) == MC_OK && api.set_preferred_sample_rate(decoder, 48000) == MC_OK,
            "select output rate");

        const mc_audio_format format { sizeof(mc_audio_format), 48000, 1, 0 };
        mc_audio_scratch_buffer* buffer = nullptr;
        check(scratch::Host().create(nullptr, &format, 0, &buffer, nullptr) == MC_OK, "create scratch");
        size_t total = 0;
        mc_decode_state state = MC_DECODE_MORE;
        while (state != MC_DECODE_END) {
            writes = {};
            check(api.decode(decoder, buffer, &state, nullptr) == MC_OK, "resample PCM");
            mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
            check(scratch::Read(buffer, &pcm) == MC_OK && pcm.format.sample_rate == 48000 && pcm.format.channels == 1,
                "preferred rate and channel count");
            check(pcm.frame_count || state == MC_DECODE_END, "decode makes progress");
            if (pcm.frame_count) {
                check(writes.calls == 1 && writes.samples == pcm.data, "one host write directly into published PCM");
            }
            for (size_t i = 0; i < pcm.frame_count; ++i) {
                check(std::abs(pcm.data[i] - 0.25f) < 0.001f, "resampled amplitude");
            }
            total += pcm.frame_count;
            check(total <= 48000, "bounded output duration");
        }
        check(total == 48000, "drain all delayed frames");
        scratch::Host().destroy(nullptr, &buffer);
        api.destroy(decoder);
        input.api->destroy(input.instance);
    }

    /* this provider reports EOF with the last bytes. a second read before
    seeking is a contract failure. hide its length so the WAV demuxer has to
    discover EOF by reading instead of deriving it from the file size. */
    struct TerminalInput {
        const mc_stream_vtable* file;
        uint64_t length;
        bool ended = false;
        bool invalidState = false;
        unsigned endings = 0;
    };
    thread_local TerminalInput probe {};

    /* play the final PCM twice to verify that seeking resets the terminal latch. */
    void checkWaveSeekAfterEnd(const mc_decoder_vtable& api, void* decoder) {
        const mc_audio_format format { sizeof(mc_audio_format), 32000, 1, 0 };
        mc_audio_scratch_buffer* buffer = nullptr;
        check(scratch::Host().create(nullptr, &format, 0, &buffer, nullptr) == MC_OK, "allocate");

        for (unsigned pass = 0; pass < 2; ++pass) {
            size_t frames = 0;
            mc_decode_state state = MC_DECODE_MORE;
            while (state != MC_DECODE_END) {
                check(api.decode(decoder, buffer, &state, nullptr) == MC_OK, "decode through terminal bytes");
                mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
                check(scratch::Read(buffer, &pcm) == MC_OK, "read decoded samples");
                frames += pcm.frame_count;
                check(frames <= 32000, "bounded track");
            }
            check(frames == 32000 && probe.endings > pass, "all terminal samples consumed");
            if (pass == 0) {
                double actual = -1;
                check(api.seek(decoder, 0, &actual, nullptr) == MC_OK && actual == 0, "seek after EOF");
            }
        }
        scratch::Host().destroy(nullptr, &buffer);
    }

    /* both codecs must consume the bytes accompanying EOF and validate the read state. */
    void checkReadStates(
        const mc_decoder_factory& factory, const mc_plugin_services& services, const std::string& path, bool wave)
    {
        mc_stream file = MC_STREAM_DEFAULT;
        check(musik::core::io::CreateLocalFileSource(file) == MC_OK, "create input");
        check(file.api->open(file.instance, { path.data(), path.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open input");
        auto methods = *file.api;
        probe = { file.api, fs::file_size(fs::u8path(path)) };
        methods.get_info = [](void* instance, mc_stream_info* info) {
            const auto status = probe.file->get_info(instance, info);
            info->flags &= ~MC_STREAM_LENGTH_KNOWN;
            info->length_bytes = 0;
            return status;
        };
        methods.seek = [](void* instance, uint64_t offset, mc_error** error) {
            const auto status = probe.file->seek(instance, offset, error);
            if (status == MC_OK) {
                probe.ended = false;
            }
            return status;
        };
        methods.read = [](void* instance, void* dst, size_t size, size_t* count, mc_read_state* state,
                           mc_error** error) -> mc_result {
            check(!probe.ended, "decoder asked for more bytes after provider's terminal read");
            const auto status = probe.file->read(instance, dst, size, count, state, error);
            uint64_t position = 0;
            check(probe.file->tell(instance, &position) == MC_OK, "read position");
            if (probe.invalidState && *count) {
                *state = UINT32_MAX;
            }
            else if (position == probe.length && *count) {
                *state = MC_READ_END;
                probe.ended = true;
                ++probe.endings;
            }
            return status;
        };
        file.api = &methods;
        const auto& api = *factory.instance_api;
        void* decoder = nullptr;
        check(factory.create(factory.context, &services, &decoder, nullptr) == MC_OK, "create decoder");
        check(api.open(decoder, &file, nullptr) == MC_OK, "open lengthless audio");
        if (wave) {
            checkWaveSeekAfterEnd(api, decoder);
        }
        else {
            check(probe.endings > 0, "module consumed its final input bytes");
        }
        api.destroy(decoder);

        /* a provider must use a documented read state even when it returned bytes. */
        check(methods.seek(file.instance, 0, nullptr) == MC_OK, "rewind");
        probe.invalidState = true;
        decoder = nullptr;
        check(factory.create(factory.context, &services, &decoder, nullptr) == MC_OK, "fresh decoder");
        check(api.open(decoder, &file, nullptr) == MC_ERROR_PLUGIN, "unknown read state rejected");
        api.destroy(decoder);
        file.api->destroy(file.instance);
    }

    /* a stream claims only complete, nonrecursive selectors. a numeric HTTP
    host is a valid container even though it resembles a nested track number. */
    void checkSelectors(PluginRegistry& registry, const char* plugin, bool gme) {
        const std::string prefix = gme ? "gme://" : "libopenmpt://";
        const std::string file = gme ? "track.nsf" : "track.mod";
        const auto source = registry.Find<mc_stream_factory>(plugin);
        check(source.Invoke([&](const auto& factory) {
            const auto claims = [&](const std::string& suffix) {
                const auto uri = prefix + suffix;
                mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
                hint.uri = { uri.data(), uri.size() };
                uint32_t supported = 0;
                check(factory.can_handle(factory.context, &hint, &supported) == MC_OK, "probe selector");
                return supported != 0;
            };
            check(claims("0/http://123/" + file), "numeric HTTP host");
            check(!claims("1xyz/" + file) && !claims("-1/" + file), "invalid subtrack number");
            check(!claims("0/") && !claims("0/another-format://1/" + file), "missing or nested container");
            return MC_OK;
        }) == MC_OK, "registered subtrack source");
    }

    void run(PluginRegistry& registry, const mc_plugin_services& services, const char* plugin, const std::string& uri,
        bool gme, bool validTrack) {
        const mc_string_view uriView { uri.data(), uri.size() };
        const auto source = registry.Find<mc_stream_factory>(plugin);
        const auto codec = registry.Find<mc_decoder_factory>(plugin);
        check(bool(source) && bool(codec), "stream and decoder factories registered in one DLL");

        mc_stream stream = MC_STREAM_DEFAULT;
        check(source.Invoke([&](const auto& factory) {
            mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
            hint.uri = uriView;
            uint32_t supported = 0;
            check(factory.can_handle(factory.context, &hint, &supported) == MC_OK && supported, "virtual URI selected");
            stream.api = factory.instance_api;
            return factory.create(factory.context, &services, &stream.instance, nullptr);
        }) == MC_OK,
            "create source");
        check(stream.api->open(stream.instance, uriView, MC_STREAM_OPEN_READ, nullptr) == MC_OK, "open container");
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        check(stream.api->get_info(stream.instance, &info) == MC_OK &&
                std::string_view(info.uri.data, info.uri.length) == uri,
            "decoder receives subsong URI");

        mc_decoder decoder = MC_DECODER_DEFAULT;
        check(codec.Invoke([&](const auto& factory) {
            mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
            hint.extension = info.extension;
            uint32_t supported = 0;
            check(factory.can_handle(factory.context, &hint, &supported) == MC_OK && supported,
                "container type retained");
            decoder.api = factory.instance_api;
            return factory.create(factory.context, &services, &decoder.instance, nullptr);
        }) == MC_OK,
            "create codec");
        const auto opened = decoder.api->open(decoder.instance, &stream, nullptr);
        check(opened == (validTrack ? MC_OK : MC_ERROR_IO), "subsong selection checked against real file");

        if (validTrack) {
            const mc_audio_format format { sizeof(mc_audio_format), 48000, 2, 0 };
            mc_audio_scratch_buffer* buffer = nullptr;
            check(services.audio_scratch->create(nullptr, &format, 0, &buffer, nullptr) == MC_OK, "create scratch");
            mc_decode_state state = MC_DECODE_MORE;
            check(decoder.api->decode(decoder.instance, buffer, &state, nullptr) == MC_OK, "decode");
            mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
            check(
                scratch::Read(buffer, &pcm) == MC_OK && pcm.format.channels == 2 && pcm.frame_count > 0, "stereo PCM");
            if (gme) {
                check(pcm.frame_count == 1024 && state == MC_DECODE_MORE, "full GME block");
                check(decoder.api->decode(decoder.instance, buffer, &state, nullptr) == MC_OK &&
                        scratch::Read(buffer, &pcm) == MC_OK && pcm.frame_count == 176 && state == MC_DECODE_END,
                    "GME tail");
            }
            services.audio_scratch->destroy(nullptr, &buffer);
        }
        decoder.api->destroy(decoder.instance);
        stream.api->destroy(stream.instance);
    }
}

/* exercise the shipped provider through both registered factories. the test
injects only the host's source creator and preferences, as an embedded host can. */
int main(int argc, char** argv) {
    check(argc == 3, "expected fixture directory and family");
    const std::string_view family = argv[2];
    const bool gme = family == "gme";
    const bool ffmpeg = family == "ffmpeg";
    const char* plugin = ffmpeg ? "f993ec34-ab43-4c6a-9a0a-6462b4ae1a1c"
        : gme                   ? "org.musikcube.decoder.gme"
                                : "org.musikcube.decoder.openmpt";
    auto file = fs::u8path(argv[1]) / (ffmpeg ? "rate.wav" : gme ? "selection.nsf" : "selection.mod");
    const auto utf8 = file.u8string();
    const std::string path(utf8.begin(), utf8.end());
    mc_media_host_api media = MC_MEDIA_HOST_API_DEFAULT;
    media.create_stream = [](void*, mc_string_view, mc_stream* stream, mc_error**) {
        return musik::core::io::CreateLocalFileSource(*stream);
    };
    mc_preferences_host_api prefs = MC_PREFERENCES_HOST_API_DEFAULT;
    prefs.read_number = readNumber;
    prefs.read_boolean = [](void*, mc_string_view, bool fallback, bool* result) -> mc_result {
        *result = fallback;
        return MC_OK;
    };
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.audio_scratch = &scratch::Host();
    services.media = &media;
    services.preferences = &prefs;
    PluginRegistry registry;
    const auto plugins = (fs::u8path(argv[0]).parent_path() / "plugins").u8string();
    check(registry.Initialize(std::string(plugins.begin(), plugins.end()),
              [&](const auto& info) {
                  return info.id == plugin;
              }) == MC_OK,
        "start registry");
    if (ffmpeg) {
        writeWave(file);
        const auto factory = registry.Find<mc_decoder_factory>(plugin);
        check(factory.Invoke([&](const auto& api) {
            checkResampler(api, services, path);
            writeWave(file, true);
            checkReadStates(api, services, path, true);
            return MC_OK;
        }) == MC_OK,
            "registered FFmpeg decoder");
    }
    else {
        checkSelectors(registry, plugin, gme);
        const auto bytes = gme ? silentNsf() : silentMod();
        std::ofstream(file, std::ios::binary).write(bytes.data(), bytes.size());
        if (!gme) {
            const auto factory = registry.Find<mc_decoder_factory>(plugin);
            check(factory.Invoke([&](const auto& api) {
                checkReadStates(api, services, path, false);
                return MC_OK;
            }) == MC_OK, "OpenMPT stream read states");
        }
        run(registry, services, plugin, (gme ? "gme://1/" : "libopenmpt://0/") + path, gme, true);
        run(registry, services, plugin, (gme ? "gme://2/" : "libopenmpt://1/") + path, gme, false);
    }
    check(registry.Shutdown() == MC_OK, "unload");
}
