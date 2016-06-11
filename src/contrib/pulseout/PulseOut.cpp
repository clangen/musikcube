//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "PulseOut.h"
#include <iostream>

#define BUFFER_COUNT 8 /* seems to work well? */

using namespace musik::core::audio;

/* a little RAII type that ensures the main loop is locked immediately,
and automatically unlocked when the instances goes out of scope */
class MainLoopLock {
    public:
        MainLoopLock(pa_threaded_mainloop* mainLoop) {
            this->mainLoop = mainLoop;
            pa_threaded_mainloop_lock(mainLoop);
        }

        ~MainLoopLock() {
            pa_threaded_mainloop_unlock(this->mainLoop);
        }

    private:
        pa_threaded_mainloop* mainLoop;
};

/* utility method to block waiting for a pa_operation to complete */
static inline bool waitForCompletion(pa_operation *op, pa_threaded_mainloop *loop) {
    if (op) {
        pa_operation_state_t state;
        while ((state = pa_operation_get_state(op)) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(loop);
        }

        pa_operation_unref(op);
        return (state == PA_OPERATION_DONE);
    }

    return false;
}

static inline void notifyBufferCompleted(PulseOut::BufferContext* context) {
    IBufferProvider* provider = context->provider;
    IBuffer* buffer = context->buffer;
    provider->OnBufferProcessed(buffer);
}

PulseOut::PulseOut() {
    this->volume = 1.0f;

    this->bytesConsumed = 0;
    this->bytesWritten = 0;

    this->pulse.loop = NULL;
    this->pulse.context = NULL;
    this->pulse.stream = NULL;
    this->pulse.format.format = PA_SAMPLE_FLOAT32LE;
    this->pulse.format.rate = 0;
    this->pulse.format.channels = 0;

    this->quit = false;

    this->InitPulse();
}

PulseOut::~PulseOut() {
    this->Stop();
    this->DeinitPulse();
    this->quit = true;
    this->thread->join();
}

void PulseOut::Destroy() {
    delete this;
}

void PulseOut::Pause() {
    this->SetPaused(true);
}

void PulseOut::Resume() {
    this->SetPaused(false);
}

void PulseOut::SetVolume(double volume) {
    this->volume = volume;

    boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

    if (this->pulse.stream) {
        pa_cvolume vol;

        pa_cvolume_set(&vol, 2, pa_sw_volume_from_linear(volume));

        MainLoopLock loopLock(this->pulse.loop);

        pa_context_set_sink_input_volume(
            this->pulse.context,
            pa_stream_get_index(this->pulse.stream),
            &vol,
            NULL,
            NULL);
    }
}

void PulseOut::Stop() {
    std::deque<std::shared_ptr<BufferContext> > toNotify;

    {
        boost::recursive_mutex::scoped_lock bufferLock(this->mutex);
        std::swap(this->buffers, toNotify);

        if (this->pulse.stream) {
            /* unset the callback so we don't try to immediately re-fill
            the buffer. or, worse, deadlock */
            pa_stream_set_write_callback(
                this->pulse.stream, NULL, NULL);

            pa_operation* flushOp = NULL;

            {
                MainLoopLock loopLock(this->pulse.loop);

                flushOp = pa_stream_flush(
                    this->pulse.stream,
                    &PulseOut::OnPulseStreamSuccess,
                    this);

                if (flushOp) {
                    waitForCompletion(flushOp, this->pulse.loop);
                }
            }

            /* reset the callback */
            pa_stream_set_write_callback(
                this->pulse.stream,
                &PulseOut::OnPulseStreamWrite,
                this);
        }

        this->bytesConsumed = 0;
        this->bytesWritten = 0;
    }

    /* all buffers are dead. notify the IBufferProvider */
    auto it = toNotify.begin();
    while (it != toNotify.end()) {
        notifyBufferCompleted((*it).get());
        ++it;
    }
}

bool PulseOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    if (!this->pulse.stream ||
        this->pulse.format.rate != buffer->SampleRate() ||
        this->pulse.format.channels != buffer->Channels())
    {
        if (this->pulse.stream) {
            std::cerr << "PulseOut: stream switched formats; not handled\n";
            return false;
        }

        this->DeinitPulseStream();
        if (!this->InitPulseStream(buffer->SampleRate(), buffer->Channels())) {
            std::cerr << "PulseOut: could not initialize stream for playback\n";
            return false;
        }
    }

    if (this->CountBuffersWithProvider(provider) >= BUFFER_COUNT) {
        // std::cerr << "PulseOut: buffers are full!\n";
        return false;
    }

    std::shared_ptr<BufferContext> context(new BufferContext());
    context->output = this;
    context->buffer = buffer;
    context->provider = provider;
    context->lastByte = this->bytesWritten + buffer->Bytes();

    {
        boost::recursive_mutex::scoped_lock bufferLock(this->mutex);
        this->buffers.push_back(context);
    }

    MainLoopLock loopLock(this->pulse.loop);

    int error =
        pa_stream_write(
            this->pulse.stream,
            static_cast<void*>(buffer->BufferPointer()),
            buffer->Bytes(),
            NULL,
            0,
            PA_SEEK_RELATIVE);

    if (error) {
        std::cerr << "PulseOut: FAILED!! this should not happen." << error << std::endl;
        notifyBufferCompleted(context.get());
    }
    else {
        boost::recursive_mutex::scoped_lock bufferLock(this->mutex);
        this->bytesWritten += buffer->Bytes();
    }

    return !error;
}

void PulseOut::SetPaused(bool paused) {
    boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

    if (this->pulse.stream) {
        pa_operation* corkOp = NULL;

        {
            MainLoopLock loopLock(this->pulse.loop);

            corkOp = pa_stream_cork(
                this->pulse.stream,
                paused ? 1 : 0,
                &PulseOut::OnPulseStreamSuccess,
                this);

            if (corkOp) {
                waitForCompletion(corkOp, this->pulse.loop);
            }
        }
    }
}

void PulseOut::OnPulseStreamWrite(pa_stream *s, size_t bytes, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);
    std::list<std::shared_ptr<BufferContext> > toNotify;

    {
        boost::recursive_mutex::scoped_lock bufferLock(out->mutex);

        out->bytesConsumed += bytes;

        auto it = out->buffers.begin();
        while (it != out->buffers.end()) {
            if (out->bytesConsumed >= (*it)->lastByte) {
                toNotify.push_back(*it);
                it = out->buffers.erase(it);
            }
            else {
                break;
            }
        }
    }

    /* actually notify here */
    auto it = toNotify.begin();
    while (it != toNotify.end()) {
        notifyBufferCompleted((*it).get());
        ++it;
    }

    // std::cerr << bytes << std::endl;
}


void PulseOut::OnPulseContextStateChanged(pa_context *context, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);

    const pa_context_state_t state = pa_context_get_state(context);

    std::cerr << "PulseOut: context connection state changed: " << state << std::endl;

    switch (state) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(out->pulse.loop, 0);
        default:
            return;
    }
}

void PulseOut::OnPulseStreamStateChanged(pa_stream *stream, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);

    const pa_stream_state_t state = pa_stream_get_state(stream);

    std::cerr << "PulseOut: stream connection state changed: " << state << std::endl;

    switch (state) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(out->pulse.loop, 0);
        default:
            return;
    }
}

void PulseOut::OnPulseStreamSuccess(pa_stream *s, int success, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);
    pa_threaded_mainloop_signal(out->pulse.loop, 0);
}

void PulseOut::InitPulse() {
    if (!this->InitPulseEventLoopAndContext()) {
        this->DeinitPulse();
    }
}

bool PulseOut::InitPulseEventLoopAndContext() {
    std::cerr << "PulseOut: init...\n";

    this->pulse.loop = pa_threaded_mainloop_new();

    if (this->pulse.loop) {
        std::cerr << "PulseOut: init ok, starting...\n";

        int error = pa_threaded_mainloop_start(this->pulse.loop);
        if (error) {
            pa_threaded_mainloop_free(this->pulse.loop);
            this->pulse.loop = NULL;
            return false;
        }

        std::cerr << "PulseOut: started ok.\n";
    }

    pa_mainloop_api* api = pa_threaded_mainloop_get_api(this->pulse.loop);

    MainLoopLock loopLock(this->pulse.loop);

    this->pulse.context = pa_context_new(api, "musikcube");

    if (this->pulse.context) {
        std::cerr << "PulseOut: context created";

        pa_context_set_state_callback(
            this->pulse.context,
            &PulseOut::OnPulseContextStateChanged,
            this);

        int error =
            pa_context_connect(
                this->pulse.context,
                NULL,
                PA_CONTEXT_NOFAIL,
                NULL);

        bool connected = false;
        while (!error && !connected) {
            pa_context_state_t state =
                pa_context_get_state(this->pulse.context);

            if (state == PA_CONTEXT_READY) {
                std::cerr << "PulseOut: connected!\n";
                connected = true;
            }
            else if (!PA_CONTEXT_IS_GOOD(state)) {
                std::cerr << "PulseOut: corrupted state! bailing.\n";
                error = true;
            }
            else {
                std::cerr << "PulseOut: waiting for connection...\n";
                pa_threaded_mainloop_wait(this->pulse.loop);
            }
        }

        if (connected && !error) {
            return true;
        }
    }

    return false;
}

bool PulseOut::InitPulseStream(size_t rate, size_t channels) {
    bool ready = false;

    {
        MainLoopLock loopLock(this->pulse.loop);

        this->pulse.format.rate = rate;
        this->pulse.format.channels = channels;

        this->pulse.stream = pa_stream_new(
            this->pulse.context,
            "musikcube PulseOut stream",
            &this->pulse.format,
            NULL); /* channel mapping */

        std::cerr << "PulseOut: creating stream...\n";

        if (this->pulse.stream) {
            std::cerr << "PulseOut: stream created.\n";

            pa_stream_set_state_callback(
                this->pulse.stream,
                &PulseOut::OnPulseStreamStateChanged,
                this);

            pa_stream_set_write_callback(
                this->pulse.stream,
                &PulseOut::OnPulseStreamWrite,
                this);

            int flags =
                PA_STREAM_AUTO_TIMING_UPDATE |
                PA_STREAM_INTERPOLATE_TIMING;

            std::cerr << "PulseOut: connecting the stream for playing...\n";

            int error = pa_stream_connect_playback(
                this->pulse.stream,
                NULL, /* device id */
                NULL, /* buffering attributes */
                (pa_stream_flags_t) flags, /* additional flags */
                NULL, /* initial volume. docs suggest NULL. */
                NULL); /* stream to synchronize with. */

            if (!error) {
                std::cerr << "PulseOut: connected. waiting for the stream to become ready\n";

                pa_threaded_mainloop_wait(this->pulse.loop);
                ready = pa_stream_get_state(this->pulse.stream) == PA_STREAM_READY;

                std::cerr << (ready ? "PulseOut: stream is ready!" : "stream failed") << std::endl;
            }
        }
    }

    if (ready) {
        this->SetVolume(this->volume);
        this->Resume();
    }

    return ready;
}

void PulseOut::DeinitPulseStream() {
    if (this->pulse.stream) {
        std::cerr << "PulseOut: freeing stream...\n";
        MainLoopLock loopLock(this->pulse.loop);
        pa_stream_disconnect(this->pulse.stream);
        pa_stream_unref(this->pulse.stream);
        this->pulse.stream = NULL;
    }
}

void PulseOut::DeinitPulse() {
    this->DeinitPulseStream();

    if (this->pulse.context) {
        std::cerr << "PulseOut: freeing context...\n";
        MainLoopLock loopLock(this->pulse.loop);
        pa_context_disconnect(this->pulse.context);
        pa_context_unref(this->pulse.context);
        this->pulse.context = NULL;
    }

    if (this->pulse.loop) {
        std::cerr << "PulseOut: stopping...\n";
        pa_threaded_mainloop_stop(this->pulse.loop);
        pa_threaded_mainloop_free(this->pulse.loop);
        this->pulse.loop = NULL;
    }
}


size_t PulseOut::CountBuffersWithProvider(IBufferProvider* provider) {
    boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

    size_t count = 0;
    auto it = this->buffers.begin();
    while (it != this->buffers.end()) {
        if ((*it)->provider == provider) {
            ++count;
        }
        ++it;
    }

    return count;
}
