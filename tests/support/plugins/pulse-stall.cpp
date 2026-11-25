#include <pulseout/pulse_blocking_stream.h>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unistd.h>

struct pa_blocking {
    std::mutex mutex;
    std::condition_variable wake;
    bool cancelled = false;
};

extern "C" pa_blocking* pa_blocking_new(const char*, const char*, pa_stream_direction_t, const char* device, const char*,
    const pa_sample_spec*, const pa_channel_map*, const pa_buffer_attr*, int*)
{
    if (device && std::strcmp(device, "unavailable") == 0) {
        return nullptr;
    }
    return new pa_blocking();
}

extern "C" void pa_blocking_cancel(pa_blocking* stream) {
    std::lock_guard lock(stream->mutex);
    stream->cancelled = true;
    stream->wake.notify_all();
}

extern "C" void pa_blocking_free(pa_blocking* stream) { delete stream; }
extern "C" int pa_blocking_cork(pa_blocking*, bool, int*) { return 0; }
extern "C" int pa_blocking_flush(pa_blocking*, int*) { return 0; }

extern "C" int pa_blocking_set_volume(pa_blocking*, int volume, int*) {
    const char* descriptor = std::getenv("MUSIKCUBE_TEST_PULSE_VOLUME_FD");
    if (descriptor) {
        const int fd = std::atoi(descriptor);
        if (write(fd, &volume, sizeof(volume)) != sizeof(volume)) {
            return -1;
        }
    }
    return 0;
}

static int stall(pa_blocking* stream, const char* mode) {
    const char* selected = std::getenv("MUSIKCUBE_TEST_PULSE_STALL");
    if (!selected || std::strcmp(selected, mode)) {
        return 0;
    }
    const char* descriptor = std::getenv("MUSIKCUBE_TEST_PULSE_GATE_FD");
    if (!descriptor) {
        return -1;
    }
    const char signal = 'x';
    if (write(std::atoi(descriptor), &signal, 1) != 1) {
        return -1;
    }
    std::unique_lock lock(stream->mutex);
    stream->wake.wait(lock, [&] { return stream->cancelled; });
    return -1;
}

extern "C" int pa_blocking_write(pa_blocking* stream, const void*, size_t, int*) {
    return stall(stream, "write");
}

extern "C" int pa_blocking_drain(pa_blocking* stream, int*) {
    return stall(stream, "drain");
}
