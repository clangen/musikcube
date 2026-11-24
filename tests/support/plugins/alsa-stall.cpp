#include <alsa/asoundlib.h>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace {
    std::atomic<int> stalled { 0 };
    std::atomic<unsigned> requestedLatency { 0 };
    std::atomic<float> lastSample { 0.0f };
    int emptyFd = -1;
    int notifyFd = -1;

    void* real(const char* name) {
        static void* library = dlopen("libasound.so.2", RTLD_NOW | RTLD_NOLOAD);
        return library ? dlsym(library, name) : nullptr;
    }
}

extern "C" int snd_pcm_open(snd_pcm_t** pcm, const char* name, snd_pcm_stream_t stream, int mode) {
    if (std::strcmp(name, "unavailable") == 0) {
        return -ENODEV;
    }
    if (std::strcmp(name, "default") == 0) {
        name = "null";
    }
    using Method = int (*)(snd_pcm_t**, const char*, snd_pcm_stream_t, int);
    return reinterpret_cast<Method>(real("snd_pcm_open"))(pcm, name, stream, mode);
}

extern "C" unsigned mc_test_alsa_latency() {
    return requestedLatency.load();
}

extern "C" float mc_test_alsa_last_sample() {
    return lastSample.load();
}

extern "C" int snd_pcm_set_params(snd_pcm_t* pcm, snd_pcm_format_t format, snd_pcm_access_t access,
    unsigned channels, unsigned rate, int resample, unsigned latency)
{
    requestedLatency.store(latency);
    using Method = int (*)(snd_pcm_t*, snd_pcm_format_t, snd_pcm_access_t, unsigned, unsigned, int, unsigned);
    return reinterpret_cast<Method>(real("snd_pcm_set_params"))(pcm, format, access, channels, rate, resample, latency);
}

extern "C" void mc_test_alsa_stall(int notify) {
    emptyFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    notifyFd = notify;
    stalled.store(1);
}

extern "C" void mc_test_alsa_stall_drain(int notify) {
    emptyFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    notifyFd = notify;
    stalled.store(2);
}

extern "C" void mc_test_alsa_unstall() {
    stalled.store(false);
    if (emptyFd >= 0) {
        close(emptyFd);
        emptyFd = -1;
    }
}

extern "C" int snd_pcm_poll_descriptors_count(snd_pcm_t* pcm) {
    if (stalled.load()) {
        return 1;
    }
    using Method = int (*)(snd_pcm_t*);
    return reinterpret_cast<Method>(real("snd_pcm_poll_descriptors_count"))(pcm);
}

extern "C" int snd_pcm_poll_descriptors(snd_pcm_t* pcm, struct pollfd* fds, unsigned int count) {
    if (stalled.load()) {
        if (!count) {
            return -EINVAL;
        }
        fds[0] = { .fd = emptyFd, .events = POLLIN, .revents = 0 };
        return 1;
    }
    using Method = int (*)(snd_pcm_t*, struct pollfd*, unsigned int);
    return reinterpret_cast<Method>(real("snd_pcm_poll_descriptors"))(pcm, fds, count);
}

extern "C" snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t* pcm, const void* data, snd_pcm_uframes_t frames) {
    if (stalled.load() == 1) {
        const char byte = 'w';
        (void) write(notifyFd, &byte, 1);
        return -EAGAIN;
    }
    if (frames) {
        lastSample.store(*static_cast<const float*>(data));
    }
    using Method = snd_pcm_sframes_t (*)(snd_pcm_t*, const void*, snd_pcm_uframes_t);
    return reinterpret_cast<Method>(real("snd_pcm_writei"))(pcm, data, frames);
}

extern "C" int snd_pcm_drain(snd_pcm_t* pcm) {
    if (stalled.load() == 2) {
        const char byte = 'd';
        (void) write(notifyFd, &byte, 1);
        return -EAGAIN;
    }
    using Method = int (*)(snd_pcm_t*);
    return reinterpret_cast<Method>(real("snd_pcm_drain"))(pcm);
}
