#include "../../../src/plugins/cddadecoder/CddaStream.h"
#include "../../../src/plugins/cddadecoder/CddaMediaStatus.h"
#include "../../../src/plugins/cddadecoder/CddaPcmDecoder.h"
#include "../../../src/plugins/cddadecoder/CddaMetadata.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

struct mc_audio_scratch_buffer {
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    std::vector<float> samples;
    size_t frames = 0;
};

namespace {
    void Require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "CDDA stream: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL SetFormat(void*, mc_audio_scratch_buffer* buffer, const mc_audio_format* format) {
        buffer->format = *format;
        return MC_OK;
    }

    mc_result MC_CALL Reserve(void*, mc_audio_scratch_buffer* buffer, size_t frames) {
        buffer->samples.resize(frames * buffer->format.channels);
        return MC_OK;
    }

    mc_result MC_CALL Begin(void*, mc_audio_scratch_buffer* buffer, mc_audio_buffer_view* view) {
        view->data = buffer->samples.data();
        return MC_OK;
    }

    mc_result MC_CALL End(void*, mc_audio_scratch_buffer* buffer, size_t frames) {
        buffer->frames = frames;
        return MC_OK;
    }

    mc_result MC_CALL StreamInfo(void* instance, mc_stream_info* info) {
        return static_cast<CddaStream*>(instance)->Info(*info);
    }

    mc_result MC_CALL StreamRead(
        void* instance, void* destination, size_t capacity, size_t* bytes, mc_read_state* state, mc_error**) {
        return static_cast<CddaStream*>(instance)->Read(destination, capacity, *bytes, *state);
    }

    mc_result MC_CALL StreamSeek(void* instance, uint64_t offset, mc_error**) {
        return static_cast<CddaStream*>(instance)->Seek(offset);
    }
}

int main(int argc, char** argv) {
    Require(CddaDriveTypeUncertain(DRIVE_UNKNOWN) && !CddaDriveTypeUncertain(DRIVE_NO_ROOT_DIR),
        "unknown drive status preserves indexed media");
    Require(CddaFindFailureUncertain(ERROR_NOT_READY, false) &&
            CddaFindFailureUncertain(ERROR_ACCESS_DENIED, false) &&
            !CddaFindFailureUncertain(ERROR_NO_MEDIA_IN_DRIVE, false) &&
            !CddaFindFailureUncertain(ERROR_NOT_READY, true),
        "transient media failures preserve entries unless removal was observed");
    CddaVolumeChanges changes;
    constexpr uint32_t gDrive = uint32_t { 1 } << ('G' - 'A');
    changes.Notify(gDrive, true);
    const auto oldRemoval = changes.Snapshot();
    changes.ConfirmPresent('G', oldRemoval);
    Require(!changes.Removed('G'), "observed media clears stale removal evidence");
    const auto beforeNewRemoval = changes.Snapshot();
    changes.Notify(gDrive, true);
    changes.ConfirmPresent('G', beforeNewRemoval);
    Require(changes.Removed('G'), "a concurrent removal survives a successful scan");
    changes.Notify(gDrive, false);
    Require(!changes.Removed('G'), "arrival clears removal evidence");
    /* MusicBrainz publishes this TOC and its expected disc ID as a reference
    vector. It exercises the exact hash input and its URL-safe Base64 alphabet. */
    const uint32_t offsets[] { 150, 15363, 32314, 46592, 63414, 80489, 95462 };
    CddaDiscInfo reference;
    for (unsigned i = 0; i < 6; ++i) {
        reference.tracks.push_back({ i + 1, offsets[i], offsets[i + 1], true });
    }
    Require(CddaMusicBrainzDiscId(reference) == "49HHV7Eb8UKF3aQiNmu1GR8vKTY-", "MusicBrainz reference disc ID");

    const std::string metadataResponse = R"({
        "releases": [{
            "title": "Example album",
            "artist-credit": [{"name": "Example artist"}],
            "genres": [],
            "release-group": {"genres": [
                {"name": "electronic", "count": 2},
                {"name": "big beat", "count": 5}
            ]},
            "media": [{"track-count": 2, "tracks": [
                {"position": 1, "title": "First"},
                {"position": 2, "title": "Second"}
            ]}]
        }]
    })";
    CddaMetadata parsed;
    Require(ParseCddaMusicBrainzResponse(metadataResponse, 2, parsed) && parsed.album == "Example album" &&
        parsed.artist == "Example artist" && parsed.genre == "big beat" && parsed.titles[2] == "Second",
        "parse album, artist, voted genre, and track titles");

    CddaStream cancelled;
    cancelled.Interrupt();
    Require(cancelled.Open({ "G:\\Track01.cda", 14 }, MC_STREAM_OPEN_READ) == MC_ERROR_CANCELLED,
        "interrupt before open remains latched");
    std::array<unsigned char, 4> cancelledBytes {};
    size_t cancelledCount = 0;
    mc_read_state cancelledState = MC_READ_MORE;
    Require(cancelled.Read(cancelledBytes.data(), cancelledBytes.size(), cancelledCount, cancelledState) ==
            MC_ERROR_CANCELLED,
        "an interrupted empty stream reports cancellation instead of EOF");

    mc_plugin_services formatServices = MC_PLUGIN_SERVICES_DEFAULT;
    CddaPcmDecoder formatDecoder(formatServices);
    mc_audio_format initialFormat = MC_AUDIO_FORMAT_DEFAULT;
    Require(formatDecoder.GetFormat(initialFormat) == MC_OK && initialFormat.sample_rate == 44100 &&
            initialFormat.channels == 2,
        "CD PCM format is available before reading audio");

    if (argc < 2) {
        std::puts("No physical track supplied; device checks skipped.");
        return 0;
    }

    const std::string path(argv[1]);
    CddaStream stream;
    Require(stream.Open({ path.data(), path.size() }, MC_STREAM_OPEN_READ) == MC_OK, "open audio track");

    if (argc > 2 && std::strcmp(argv[2], "--metadata") == 0) {
        std::stop_source interrupted;
        CddaMetadata metadata;
        if (LookupCddaMetadata(stream.Disc(), interrupted.get_token(), metadata)) {
            std::printf("MusicBrainz: %s - %s (%zu tracks; genre: %s)\n", metadata.artist.c_str(),
                metadata.album.c_str(), metadata.titles.size(), metadata.genre.c_str());
        }
        else {
            std::puts("MusicBrainz returned no usable match; generic labels will be used.");
        }
    }

    /* Existing library entries use the older short track name. */
    if (path.size() == 14) {
        const std::string legacyPath = path.substr(0, 3) + path.substr(8);
        CddaStream legacy;
        Require(legacy.Open({ legacyPath.data(), legacyPath.size() }, MC_STREAM_OPEN_READ) == MC_OK,
            "open a legacy CDDA track URI");
    }

    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    Require(stream.Info(info) == MC_OK && info.length_bytes > 5000 && (info.flags & MC_STREAM_SEEKABLE),
        "track reports length and seek support");

    if (argc > 2 && std::strcmp(argv[2], "--benchmark") == 0) {
        /* Measure a reposition followed by sequential transfers on the actual
        drive. This is opt-in because it moves the drive's read head. */
        const uint64_t offset = ((info.length_bytes / 2) / 2352) * 2352;
        const auto seekStarted = std::chrono::steady_clock::now();
        Require(stream.Seek(offset) == MC_OK, "seek for transfer timing");
        const auto seekElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - seekStarted).count();
        std::printf("CD seek and initial read: %lld ms\n", static_cast<long long>(seekElapsed));
        std::vector<unsigned char> transfer(16 * 2352);
        const auto started = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 20; ++i) {
            size_t count = 0;
            mc_read_state readState = MC_READ_MORE;
            const auto before = std::chrono::steady_clock::now();
            Require(stream.Read(transfer.data(), transfer.size(), count, readState) == MC_OK && count == transfer.size(),
                "measure sequential CD reads");
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - before).count();
            std::printf("CD transfer %u: %lld ms\n", i, static_cast<long long>(elapsed));
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::printf("CD transfer total: %lld ms\n", static_cast<long long>(elapsed));
        Require(stream.Seek(0) == MC_OK, "rewind after transfer timing");
    }

    std::array<unsigned char, 5000> original {};
    size_t bytes = 0;
    mc_read_state state = MC_READ_MORE;
    Require(stream.Read(original.data(), original.size(), bytes, state) == MC_OK && bytes == original.size(),
        "read across raw sector boundaries");

    Require(stream.Seek(2300) == MC_OK, "seek inside a sector");
    std::array<unsigned char, 1000> reread {};
    Require(stream.Read(reread.data(), reread.size(), bytes, state) == MC_OK && bytes == reread.size() &&
            std::memcmp(reread.data(), original.data() + 2300, reread.size()) == 0,
        "arbitrary byte seek preserves data across sector boundaries");

    /* Decode the actual disc bytes through the public stream and scratch
    interfaces, then compare every sample with the original raw PCM. */
    mc_stream_vtable streamApi = MC_STREAM_VTABLE_DEFAULT;
    streamApi.get_info = StreamInfo;
    streamApi.read = StreamRead;
    streamApi.seek = StreamSeek;
    mc_stream input { sizeof(mc_stream), &stream, &streamApi };
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    CddaPcmDecoder decoder(services);
    Require(stream.Seek(0) == MC_OK && decoder.Open(input) == MC_OK, "attach CD PCM decoder");
    double seconds = 0;
    Require(decoder.GetDuration(seconds) == MC_OK && seconds > 0, "decoder reports track duration");

    mc_plugin_audio_scratch_service scratch = MC_PLUGIN_AUDIO_SCRATCH_SERVICE_DEFAULT;
    scratch.set_format = SetFormat;
    scratch.reserve = Reserve;
    scratch.begin_write = Begin;
    scratch.end_write = End;
    mc_audio_scratch_buffer decoded;
    musik::core::plugin::DecoderBuffer target(scratch, decoded);
    mc_decode_state decodeState = MC_DECODE_MORE;
    Require(decoder.Decode(target, decodeState) == MC_OK && decoded.frames == 1024 &&
            decoded.format.sample_rate == 44100 && decoded.format.channels == 2,
        "decode CD stereo PCM");
    for (size_t i = 0; i < decoded.samples.size(); ++i) {
        const uint16_t raw = uint16_t(original[2 * i]) | (uint16_t(original[2 * i + 1]) << 8);
        const int32_t signedSample = raw < 0x8000 ? raw : static_cast<int32_t>(raw) - 65536;
        Require(decoded.samples[i] == static_cast<float>(signedSample) / 32768.0f, "decoded sample matches disc");
    }

    Require(stream.Seek(info.length_bytes - 4) == MC_OK, "seek to final frame");
    Require(stream.Read(reread.data(), reread.size(), bytes, state) == MC_OK && bytes == 4 && state == MC_READ_END,
        "final bytes and EOF are reported together");
    stream.Interrupt();
    Require(stream.Seek(0) == MC_ERROR_CANCELLED, "interrupted seek cannot start a device read");
    Require(stream.Read(reread.data(), reread.size(), bytes, state) == MC_ERROR_CANCELLED,
        "interrupt remains latched for later reads");
    Require(stream.Close() == MC_OK, "close track");

    /* Race interruption against an actual multi-sector drive read. A fast
    drive may complete first; otherwise interruption must release the reader. */
    CddaStream concurrent;
    Require(concurrent.Open({ path.data(), path.size() }, MC_STREAM_OPEN_READ) == MC_OK,
        "open track for concurrent interruption");
    std::vector<unsigned char> longRead(512 * 1024);
    std::atomic<bool> started { false };
    std::atomic<mc_result> result { MC_ERROR_INTERNAL };
    std::thread reader([&] {
        size_t readBytes = 0;
        mc_read_state readState = MC_READ_MORE;
        started.store(true);
        result.store(concurrent.Read(longRead.data(), longRead.size(), readBytes, readState));
    });
    while (!started.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto start = std::chrono::steady_clock::now();
    concurrent.Interrupt();
    reader.join();
    Require(result.load() == MC_ERROR_CANCELLED || result.load() == MC_OK,
        "concurrent read completes or reports cancellation");
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(5),
        "interruption releases the physical drive read promptly");
}
