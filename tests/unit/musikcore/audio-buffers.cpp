#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>

#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace musik::core::ffi;

/* assertions fail in Release too; these tests exercise growable decoder scratch
storage through the public C table, not the core's fixed playback pool,
with only Read using the core's private read-only access helper. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "audio buffers: %s\n", message);
        std::abort();
    }
}

/* a plugin writes actual samples, then publishes complete frames. changing its
local view cannot alter the host's format, capacity, or populated frame count. */
static void checkWriteAndRead() {
    const auto& host = audio_scratch_buffers::Host();
    const mc_audio_format format { sizeof(mc_audio_format), 48000, 2, 0 };
    mc_audio_scratch_buffer* buffer = nullptr;
    require(host.create(host.context, &format, 4, &buffer, nullptr) == MC_OK, "host creates opaque storage");
    mc_audio_buffer_view writable = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    require(host.begin_write(host.context, buffer, &writable) == MC_OK, "begin_write exposes stable storage");
    require(writable.frame_capacity == 4 && writable.format.channels == 2, "capacity is measured in frames");
    auto* samples = writable.data;
    for (size_t sample = 0; sample < 6; ++sample) {
        samples[sample] = static_cast<float>(sample) / 8;
    }
    writable.frame_capacity = 1000;
    writable.format.channels = 1000;
    require(host.end_write(host.context, buffer, 3) == MC_OK, "publish exactly three frames");

    mc_pcm_view read = MC_PCM_VIEW_DEFAULT;
    require(audio_scratch_buffers::Read(buffer, &read) == MC_OK && read.frame_count == 3 && read.format.channels == 2,
        "caller view mutation does not change host bookkeeping");
    require(read.data[5] == 5.0f / 8, "published PCM retains the written samples");
    require(host.reserve(host.context, buffer, 8) == MC_OK, "reserve grows storage outside a write");
    require(audio_scratch_buffers::Read(buffer, &read) == MC_OK && read.frame_count == 3 && read.data[5] == 5.0f / 8,
        "growing capacity preserves populated samples");

    /* decoders may restate the format per block; that must not discard reusable
    capacity and force another allocation before each write. */
    require(host.set_format(host.context, buffer, &format) == MC_OK, "same format can be restated");
    require(host.begin_write(host.context, buffer, &writable) == MC_OK && writable.frame_capacity >= 8,
        "same-format reset preserves reusable capacity");
    require(host.end_write(host.context, buffer, 0) == MC_OK, "finish empty reused write");
    host.destroy(host.context, &buffer);
    require(!buffer, "destroy clears ownership slot");
    host.destroy(host.context, &buffer);
}

/* incomplete writes must never escape as valid audio. reject mutation and read
access until end_write, then allow a fresh write after abandonment or bad counts. */
static void checkExclusiveAccessAndFailure() {
    const auto& host = audio_scratch_buffers::Host();
    mc_audio_format format { sizeof(mc_audio_format), 44100, 1, 0 };
    mc_audio_scratch_buffer* buffer = nullptr;
    require(host.create(host.context, &format, 2, &buffer, nullptr) == MC_OK, "create exclusive-access buffer");
    mc_audio_buffer_view first = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    mc_audio_buffer_view second = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    mc_pcm_view read = MC_PCM_VIEW_DEFAULT;
    require(host.begin_write(host.context, buffer, &first) == MC_OK, "first write begins");
    require(host.begin_write(host.context, buffer, &second) == MC_ERROR_IN_FLIGHT && !second.data,
        "nested write grants no second view");
    require(host.reserve(host.context, buffer, 8) == MC_ERROR_IN_FLIGHT, "storage cannot move during writing");
    require(
        host.set_format(host.context, buffer, &format) == MC_ERROR_IN_FLIGHT, "format cannot change during writing");
    require(audio_scratch_buffers::Read(buffer, &read) == MC_ERROR_IN_FLIGHT && !read.data, "partial audio cannot be read");
    require(host.end_write(host.context, buffer, 3) == MC_ERROR_INVALID_ARGUMENT, "over-capacity count is rejected");
    require(audio_scratch_buffers::Read(buffer, &read) == MC_OK && read.frame_count == 0, "invalid write publishes no audio");
    require(host.begin_write(host.context, buffer, &first) == MC_OK, "invalid count still ended access");
    require(host.end_write(host.context, buffer, 0) == MC_OK, "zero frames abandons a failed decode");
    require(host.end_write(host.context, buffer, 0) == MC_ERROR_INVALID_ARGUMENT, "unpaired end is rejected");

    format.channels = 2;
    require(host.set_format(host.context, buffer, &format) == MC_OK, "format changes outside a write");
    require(host.begin_write(host.context, buffer, &first) == MC_OK && first.frame_capacity == 0,
        "format change invalidates previous capacity and audio");
    require(host.end_write(host.context, buffer, 0) == MC_OK, "empty write still has a paired end");
    host.destroy(host.context, &buffer);
}

/* reserve is capacity management, not ring acquisition: smaller requests keep
the same samples and pointer, and overflow cannot disturb published audio. */
static void checkReservePreservesStorage() {
    const auto& host = audio_scratch_buffers::Host();
    const mc_audio_format format { sizeof(mc_audio_format), 48000, 2, 0 };
    mc_audio_scratch_buffer* buffer = nullptr;
    require(host.create(host.context, &format, 4, &buffer, nullptr) == MC_OK, "create scratch capacity");

    mc_audio_buffer_view writable = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    require(host.begin_write(host.context, buffer, &writable) == MC_OK, "write scratch sample");
    writable.data[0] = 0.5f;
    require(host.end_write(host.context, buffer, 1) == MC_OK, "publish scratch sample");

    mc_pcm_view before = MC_PCM_VIEW_DEFAULT;
    require(audio_scratch_buffers::Read(buffer, &before) == MC_OK, "read initial scratch storage");
    const float* original = before.data;
    require(host.reserve(host.context, buffer, 2) == MC_OK, "smaller reserve succeeds");
    require(host.reserve(host.context, buffer, 0) == MC_OK, "zero reserve does not shrink");
    require(host.reserve(host.context, buffer, std::numeric_limits<size_t>::max()) == MC_ERROR_INVALID_ARGUMENT,
        "overflow fails before changing scratch storage");

    mc_pcm_view after = MC_PCM_VIEW_DEFAULT;
    require(audio_scratch_buffers::Read(buffer, &after) == MC_OK && after.data == original &&
            after.frame_count == 1 && after.data[0] == 0.5f,
        "no-op and failed reserves preserve pointer and published PCM");
    require(host.begin_write(host.context, buffer, &writable) == MC_OK && writable.frame_capacity == 4,
        "no-op and failed reserves preserve capacity");
    require(host.end_write(host.context, buffer, 0) == MC_OK, "end capacity inspection");
    host.destroy(host.context, &buffer);
}

/* malformed layouts and overflow must fail before allocating or accessing
storage, and failure must not transfer ownership or expose a stale view. */
static void checkInvalidInputs() {
    const auto& host = audio_scratch_buffers::Host();
    mc_audio_format format { sizeof(mc_audio_format), 48000, 2, 0 };
    mc_audio_scratch_buffer* buffer = nullptr;
    require(host.create(host.context, &format, std::numeric_limits<size_t>::max(), &buffer, nullptr) ==
                MC_ERROR_INVALID_ARGUMENT &&
            !buffer,
        "overflow fails before allocation");
    format.channels = 0;
    require(host.create(host.context, &format, 1, &buffer, nullptr) == MC_ERROR_INVALID_ARGUMENT && !buffer,
        "unset channel count is invalid");
    mc_audio_buffer_view view = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    require(host.begin_write(host.context, nullptr, &view) == MC_ERROR_INVALID_ARGUMENT && !view.data,
        "null buffer grants no writable storage");
    view.struct_size = 0;
    require(host.begin_write(host.context, nullptr, &view) == MC_ERROR_INVALID_ARGUMENT,
        "truncated output view is rejected");
}

/* callers may supply a larger future format prefix. returned nested formats
must advertise only bytes the host actually stored, never the caller's tail. */
static void checkFormatPrefix() {
    struct ExtendedFormat {
        mc_audio_format format;
        uint32_t extension[4];
    } extended { { sizeof(ExtendedFormat), 48000, 2, 0 }, {} };
    const auto& host = audio_scratch_buffers::Host();
    mc_audio_scratch_buffer* buffer = nullptr;
    require(host.create(host.context, &extended.format, 1, &buffer, nullptr) == MC_OK, "accept known format prefix");
    mc_pcm_view view = MC_PCM_VIEW_DEFAULT;
    require(audio_scratch_buffers::Read(buffer, &view) == MC_OK && view.format.struct_size == sizeof(mc_audio_format),
        "returned format advertises host-owned bytes only");
    extended.format.sample_rate = 96000;
    require(host.set_format(host.context, buffer, &extended.format) == MC_OK, "replace format from extended input");
    require(audio_scratch_buffers::Read(buffer, &view) == MC_OK && view.format.struct_size == sizeof(mc_audio_format) &&
            view.format.sample_rate == 96000,
        "format reset also returns an honest prefix");
    host.destroy(host.context, &buffer);
}

/* independent buffers require no plugin library or audio device. */
int main() {
    checkWriteAndRead();
    checkExclusiveAccessAndFailure();
    checkReservePreservesStorage();
    checkInvalidInputs();
    checkFormatPrefix();
}
