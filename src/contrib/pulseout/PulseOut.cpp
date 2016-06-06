#include "PulseOut.h"
#include <iostream>

#define BUFFER_COUNT 48 /* seems to work well? */

using namespace musik::core::audio;

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

static bool waitForCompletion(pa_operation *op, pa_threaded_mainloop *loop) {
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

long long bufferLengthMicroSeconds(IBuffer* buffer) {
    long long samples = buffer->Samples();
    long long rate = buffer->SampleRate();
    return (samples * 1000000) / rate;
}

bool PulseOut::RemoveBufferFromQueue(BufferContext* context) {
    boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

    auto it = this->buffers.begin();
    while (it != this->buffers.end()) {
        if ((*it).get() == context) {
            this->buffers.erase(it);
            this->bufferQueueLength -= bufferLengthMicroSeconds((*it)->buffer);
            return true;
        }
        ++it;
    }

    return false;
}

void PulseOut::NotifyBufferCompleted(BufferContext* context) {
    IBufferProvider* provider = context->provider;
    IBuffer* buffer = context->buffer;
    provider->OnBufferProcessed(buffer);
}

PulseOut::PulseOut() {
    this->volume = 1.0f;
    this->pulseMainLoop = 0;;
    this->pulseContext = 0;
    this->pulseStream = 0;
    this->pulseStreamFormat.format = PA_SAMPLE_FLOAT32LE;
    this->pulseStreamFormat.rate = 0;
    this->pulseStreamFormat.channels = 0;
    this->bufferQueueLength = 0.0f;

    boost::thread th(boost::bind(&PulseOut::ThreadProc, this));
    th.detach();

    this->InitPulse();
}

bool PulseOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    if (!this->pulseStream ||
        this->pulseStreamFormat.rate != buffer->SampleRate() ||
        this->pulseStreamFormat.channels != buffer->Channels())
    {
        if (this->pulseStream) {
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

    {
        boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

        long long bufferLength = bufferLengthMicroSeconds(buffer);
        //std::cerr << "PulseOut: bufferLength " << bufferLength << std::endl;
        pa_usec_t currentTime;
        pa_stream_get_time(this->pulseStream, &currentTime);
        context->endTime = bufferLength + currentTime + this->bufferQueueLength;
        this->bufferQueueLength += bufferLength;

        this->buffers.push_back(context);
    }

    MainLoopLock loopLock(this->pulseMainLoop);

    int error =
        pa_stream_write_ext_free(
            this->pulseStream,
            static_cast<void*>(buffer->BufferPointer()),
            buffer->Bytes(),
            &PulseOut::OnPulseBufferPlayed,
            static_cast<void*>(context.get()),
            0,
            PA_SEEK_RELATIVE);

    if (error) {
        std::cerr << "PulseOut: FAILED!! this should not happen." << error << std::endl;
        this->NotifyBufferCompleted(context.get());
    }
    else {
        // std::cerr << "PulseOut: buffer enqueued!" << std::endl;
    }

    // std::cerr << "PulseOut: wrote " << (error ? "unsuccessfully" : "successfully") << std::endl;

    return !error;
}

PulseOut::~PulseOut() {
    this->Stop();
    this->DeinitPulse();
}

void PulseOut::ThreadProc() {
    pa_usec_t lastTime = -1;
    pa_usec_t now;

    while (true) {
        if (this->pulseStream) {
            pa_stream_get_time(this->pulseStream, &now);
            //std::cerr << "PulseOut: time now: " << now << std::endl;

            std::list<std::shared_ptr<BufferContext> > toNotify;

            {
                //std::cerr << "PulseOut: lastTime " << lastTime << " now " << now << std::endl;

                /* detect underrun */
                if (lastTime == now) {
                    std::cerr << "PulseOut: UNDERRUN!" << std::endl;
                    now = this->buffers.front()->endTime + 1;
                }

                boost::recursive_mutex::scoped_lock bufferLock(this->mutex);
                auto it = this->buffers.begin();
                while (it != this->buffers.end()) {
                    if ((*it)->endTime <= now) {
                        toNotify.push_back(*it);
                        this->bufferQueueLength -= bufferLengthMicroSeconds((*it)->buffer);
                        it = this->buffers.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                if (!toNotify.size() && this->buffers.size()) {
                    //std::cerr << "PulseOut: TOP TOO FAR AHEAD AT " << this->buffers.front()->endTime << std::endl;
                }
                else {
                    //std::cerr << "PulseOut: we think we have something...?";
                }
            }

            auto it = toNotify.begin();
            while (it != toNotify.end()) {
                this->NotifyBufferCompleted((*it).get());
                ++it;
            }

        }

        lastTime = now;
        usleep(50 * 1000);
    }
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

void PulseOut::SetPaused(bool paused) {
    if (this->pulseStream) {
        MainLoopLock loopLock(this->pulseMainLoop);
        waitForCompletion(
            pa_stream_cork(
                this->pulseStream,
                paused ? 1 : 0,
                &PulseOut::OnPulseStreamSuccessCallback,
                this),
            this->pulseMainLoop);
    }
}

void PulseOut::SetVolume(double volume) {
}

void PulseOut::Stop() {
    std::deque<std::shared_ptr<BufferContext> > toNotify;

    {
        boost::recursive_mutex::scoped_lock bufferLock(this->mutex);

        if (this->pulseStream) {
            MainLoopLock loopLock(this->pulseMainLoop);

            std::swap(this->buffers, toNotify);

            waitForCompletion(
                pa_stream_flush(
                    this->pulseStream,
                    &PulseOut::OnPulseStreamSuccessCallback,
                    this),
                this->pulseMainLoop);
        }
    }

    auto it = toNotify.begin();
    while (it != toNotify.end()) {
        this->bufferQueueLength -= bufferLengthMicroSeconds((*it)->buffer);
        this->NotifyBufferCompleted((*it).get());
        ++it;
    }
}

void PulseOut::OnPulseBufferPlayed(void *data) {
    // BufferContext* context = static_cast<BufferContext*>(data);
    // context->output->NotifyBufferCompleted(context);
}

void PulseOut::OnPulseContextStateChanged(pa_context *context, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);
    const pa_context_state_t state = pa_context_get_state(context);

    std::cerr << "PulseOut: context connection state changed: " << state << std::endl;

    switch (state) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(out->pulseMainLoop, 0);
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
            pa_threaded_mainloop_signal(out->pulseMainLoop, 0);
        default:
            return;
    }
}

void PulseOut::OnPulseStreamSuccessCallback(pa_stream *s, int success, void *data) {
    PulseOut* out = static_cast<PulseOut*>(data);
    pa_threaded_mainloop_signal(out->pulseMainLoop, 0);
}

void PulseOut::InitPulse() {
    if (!this->InitPulseEventLoopAndContext()) {
        this->DeinitPulse();
    }
}

bool PulseOut::InitPulseEventLoopAndContext() {
    std::cerr << "PulseOut: init...\n";
    this->pulseMainLoop = pa_threaded_mainloop_new();
    if (this->pulseMainLoop) {
        std::cerr << "PulseOut: init ok, starting...\n";

        int error = pa_threaded_mainloop_start(this->pulseMainLoop);
        if (error) {
            pa_threaded_mainloop_free(this->pulseMainLoop);
            this->pulseMainLoop = NULL;
            return false;
        }

        std::cerr << "PulseOut: started ok.\n";
    }

    pa_mainloop_api* api = pa_threaded_mainloop_get_api(this->pulseMainLoop);

    MainLoopLock loopLock(this->pulseMainLoop);

    this->pulseContext = pa_context_new(api, "musikcube");

    if (this->pulseContext) {
        std::cerr << "PulseOut: context created";

        pa_context_set_state_callback(
            this->pulseContext,
            &PulseOut::OnPulseContextStateChanged,
            this);

        int error =
            pa_context_connect(
                this->pulseContext,
                NULL,
                PA_CONTEXT_NOFAIL,
                NULL);

        bool connected = false;
        while (!error && !connected) {
            pa_context_state_t state =
                pa_context_get_state(this->pulseContext);

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
                pa_threaded_mainloop_wait(this->pulseMainLoop);
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
        MainLoopLock loopLock(this->pulseMainLoop);

        this->pulseStreamFormat.rate = rate;
        this->pulseStreamFormat.channels = channels;

        this->pulseStream = pa_stream_new(
            this->pulseContext,
            "musikcube PulseOut stream",
            &this->pulseStreamFormat,
            NULL); /* channel mapping */

        std::cerr << "PulseOut: creating stream...\n";

        if (this->pulseStream) {
            std::cerr << "PulseOut: stream created.\n";

            pa_stream_set_state_callback(
                this->pulseStream,
                &PulseOut::OnPulseStreamStateChanged,
                this);

            int flags =
                PA_STREAM_AUTO_TIMING_UPDATE |
                PA_STREAM_INTERPOLATE_TIMING;

            std::cerr << "PulseOut: connecting the stream for playing...\n";

            int error = pa_stream_connect_playback(
                this->pulseStream,
                NULL, /* device id */
                NULL, /* buffering attributes */
                (pa_stream_flags_t) flags, /* additional flags */
                NULL, /* initial volume. docs suggest NULL. */
                NULL); /* stream to synchronize with. */

            if (!error) {
                std::cerr << "PulseOut: connected. waiting for the stream to become ready\n";

                pa_threaded_mainloop_wait(this->pulseMainLoop);
                ready = pa_stream_get_state(this->pulseStream) == PA_STREAM_READY;

                std::cerr << (ready ? "PulseOut: stream is ready!" : "stream failed") << std::endl;
            }
        }
    }

    if (ready) {
        this->Resume();
    }

    return ready;
}

void PulseOut::DeinitPulseStream() {
    if (this->pulseStream) {
        std::cerr << "PulseOut: freeing stream...\n";
        MainLoopLock loopLock(this->pulseMainLoop);
        pa_stream_disconnect(this->pulseStream);
        pa_stream_unref(this->pulseStream);
        this->pulseStream = NULL;
    }
}

void PulseOut::DeinitPulse() {
    this->DeinitPulseStream();

    if (this->pulseContext) {
        std::cerr << "PulseOut: freeing context...\n";
        MainLoopLock loopLock(this->pulseMainLoop);
        pa_context_disconnect(this->pulseContext);
        pa_context_unref(this->pulseContext);
        this->pulseContext = NULL;
    }

    if (this->pulseMainLoop) {
        std::cerr << "PulseOut: stopping...\n";
        pa_threaded_mainloop_stop(this->pulseMainLoop);
        pa_threaded_mainloop_free(this->pulseMainLoop);
        this->pulseMainLoop = NULL;
    }
}
