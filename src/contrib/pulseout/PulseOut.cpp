#include "PulseOut.h"
#include <iostream>

#define BUFFER_COUNT 8

using namespace musik::core::audio;

size_t countBuffersWithProvider(
    const std::list<PulseOut::BufferContext*>& buffers,
    const IBufferProvider* provider)
{
    size_t count = 0;
    auto it = buffers.begin();
    while (it != buffers.end()) {
        if ((*it)->provider == provider) {
            ++count;
        }
        ++it;
    }
    return count;
}

void PulseOut::NotifyBufferCompleted(BufferContext *context) {
    boost::recursive_mutex::scoped_lock lock(this->mutex);

    auto it = this->buffers.begin();
    while (it != this->buffers.end()) {
        if (*it == context) {
            this->buffers.erase(it);
        }
        ++it;
    }

    context->provider->OnBufferProcessed(context->buffer);

    delete context;
}

PulseOut::PulseOut() {
    this->volume = 1.0f;
    this->pulseMainLoop = 0;;
    this->pulseContext = 0;
    this->pulseStream = 0;
    this->InitPulse();
}

bool PulseOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    return false;
}

PulseOut::~PulseOut() {
    this->Stop();
    this->DeinitPulse();
}

void PulseOut::Destroy() {
    delete this;
}

void PulseOut::Pause() {
}

void PulseOut::Resume() {
}

void PulseOut::SetVolume(double volume) {
}

void PulseOut::Stop() {
}

void PulseOut::OnPulseContextStateChanged(pa_context *c, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);
    const pa_context_state_t state = pa_context_get_state(c);

    std::cerr << PA_CONTEXT_READY << std::endl;

    switch (state) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(out->pulseMainLoop, 0);
        default:
            return;
    }
}

void PulseOut::InitPulse() {
    std::cerr << "init...\n";
    this->pulseMainLoop = pa_threaded_mainloop_new();
    if (this->pulseMainLoop) {
        std::cerr << "init ok, starting...\n";

        int error = pa_threaded_mainloop_start(this->pulseMainLoop);
        if (error) {
            pa_threaded_mainloop_free(this->pulseMainLoop);
            this->pulseMainLoop = 0;
            return;
        }

        std::cerr << "started ok.\n";
    }

    pa_mainloop_api* api = pa_threaded_mainloop_get_api(this->pulseMainLoop);
    this->pulseContext = pa_context_new(api, "musikcube");

    if (this->pulseContext) {
        std::cerr << "context created";

        pa_context_set_state_callback(
            this->pulseContext,
            &PulseOut::OnPulseContextStateChanged,
            this);

        pa_context_connect(
            this->pulseContext,
            NULL,
            PA_CONTEXT_NOFAIL,
            NULL);
    }
}

void PulseOut::DeinitPulse() {
    if (this->pulseMainLoop) {
        std::cerr << "stopping...\n";
        pa_threaded_mainloop_stop(this->pulseMainLoop);
        pa_threaded_mainloop_free(this->pulseMainLoop);
        this->pulseMainLoop = 0;
        std::cerr << "stopped\n";
    }
}
