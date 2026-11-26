#include <sndio.h>

#include <cstring>

namespace {
    struct Device {
        sio_par parameters {};
        void (*onMove)(void*, int) = nullptr;
        void* context = nullptr;
    };

    unsigned opens = 0;

    Device* Get(sio_hdl* handle) {
        return reinterpret_cast<Device*>(handle);
    }
}

extern "C" unsigned musikcore_test_sndio_opens() {
    return opens;
}

extern "C" sio_hdl* sio_open(const char*, unsigned mode, int nonblocking) {
    if (mode != SIO_PLAY || !nonblocking) {
        return nullptr;
    }
    ++opens;
    return reinterpret_cast<sio_hdl*>(new Device);
}

extern "C" void sio_close(sio_hdl* handle) {
    delete Get(handle);
}

extern "C" void sio_initpar(sio_par* parameters) {
    std::memset(parameters, 0, sizeof(*parameters));
}

extern "C" int sio_setpar(sio_hdl* handle, sio_par* parameters) {
    Get(handle)->parameters = *parameters;
    return 1;
}

extern "C" int sio_getpar(sio_hdl* handle, sio_par* parameters) {
    *parameters = Get(handle)->parameters;
    return 1;
}

extern "C" void sio_onmove(sio_hdl* handle, void (*callback)(void*, int), void* context) {
    Get(handle)->onMove = callback;
    Get(handle)->context = context;
}

extern "C" size_t sio_write(sio_hdl* handle, const void*, size_t length) {
    auto* device = Get(handle);
    if (device->onMove) {
        device->onMove(device->context, static_cast<int>(length / (device->parameters.bps * device->parameters.pchan)));
    }
    return length;
}

extern "C" int sio_start(sio_hdl*) {
    return 1;
}
extern "C" int sio_stop(sio_hdl*) {
    return 1;
}
extern "C" int sio_flush(sio_hdl*) {
    return 1;
}
extern "C" int sio_nfds(sio_hdl*) {
    return 0;
}
extern "C" int sio_pollfd(sio_hdl*, pollfd*, int) {
    return 0;
}
extern "C" int sio_revents(sio_hdl*, pollfd*) {
    return 0;
}
extern "C" int sio_eof(sio_hdl*) {
    return 0;
}
