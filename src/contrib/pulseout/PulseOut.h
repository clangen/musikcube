#pragma once

#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>
#include <deque>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <pulse/pulseaudio.h>

class PulseOut : public musik::core::audio::IOutput {
    public:
        PulseOut();
        virtual ~PulseOut();

        virtual void Destroy();
        virtual void Pause();
        virtual void Resume();
        virtual void SetVolume(double volume);
        virtual void Stop();

        virtual bool Play(
            musik::core::audio::IBuffer *buffer,
            musik::core::audio::IBufferProvider *provider);

    private:
        struct BufferContext {
            PulseOut *output;
            musik::core::audio::IBuffer *buffer;
            musik::core::audio::IBufferProvider *provider;
            long long endTime;
        };

        static void OnPulseContextStateChanged(pa_context *c, void *data);
        static void OnPulseStreamStateChanged(pa_stream *s, void *data);
        static void OnPulseStreamSuccessCallback(pa_stream *s, int success, void *data);
        static void OnPulseBufferPlayed(void *data);

        void ThreadProc(); /* ugh shoot me */

        void NotifyBufferCompleted(BufferContext *context);
        size_t CountBuffersWithProvider(musik::core::audio::IBufferProvider *provider);

        void InitPulse();
        bool InitPulseEventLoopAndContext();
        bool InitPulseStream(size_t rate, size_t channels);
        void DeinitPulseStream();
        void DeinitPulse();
        void SetPaused(bool paused);

        double volume;
        std::deque<std::shared_ptr<BufferContext> > buffers;
        std::unique_ptr<boost::thread> thread;
        boost::recursive_mutex mutex;
        pa_threaded_mainloop* pulseMainLoop;
        pa_context* pulseContext;
        pa_stream* pulseStream;
        pa_sample_spec pulseStreamFormat;
        double bufferQueueLength;
        volatile bool quit;
};
