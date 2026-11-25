#include <sndioout/SndioOut.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
    sio_par negotiated;
    unsigned opens = 0;
    unsigned closes = 0;
    int handle;
}

extern "C" sio_hdl* sio_open(const char*, unsigned, int) {
    ++opens;
    return reinterpret_cast<sio_hdl*>(&handle);
}
extern "C" void sio_close(sio_hdl*) {
    ++closes;
}
extern "C" void sio_initpar(sio_par* parameters) {
    std::memset(parameters, 0, sizeof(*parameters));
}
extern "C" int sio_setpar(sio_hdl*, sio_par* parameters) {
    negotiated = *parameters;
    return 1;
}
extern "C" int sio_getpar(sio_hdl*, sio_par* parameters) {
    *parameters = negotiated;
    return 1;
}
extern "C" int sio_nfds(sio_hdl*) {
    return 1;
}
extern "C" void sio_onmove(sio_hdl*, void (*)(void*, int), void*) {}
extern "C" int sio_start(sio_hdl*) {
    return 1;
}
/* the older-library build must never attempt this unavailable operation. */
extern "C" int sio_flush(sio_hdl*) {
    std::abort();
}

int main() {
    SndioOut output(nullptr);
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format.sample_rate = 48000;
    options.format.channels = 2;
    options.queue_capacity_frames = 16;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    if (output.Prepare(options, callbacks) != MC_OK || output.Flush() != MC_ERROR_UNSUPPORTED ||
        output.Stop() != MC_OK || opens != 1 || closes != 1 ||
        output.Prepare(options, callbacks) != MC_OK || output.Stop() != MC_OK || opens != 2 || closes != 2)
    {
        std::fputs("sndio without flush did not stop and reopen safely\n", stderr);
        return 1;
    }
    return 0;
}
