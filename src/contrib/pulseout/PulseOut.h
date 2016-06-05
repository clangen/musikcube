#pragma once

#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>
#include <queue>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <pulse/pulseaudio.h>

class PulseOut : public musik::core::audio::IOutput {
    public:
        struct BufferContext {
            musik::core::audio::IBuffer *buffer;
            musik::core::audio::IBufferProvider *provider;
        };

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

        void NotifyBufferCompleted(BufferContext *context);

    private:
        static void OnPulseContextStateChanged(pa_context *c, void *data);

        void InitPulse();
        void DeinitPulse();

        double volume;
        std::list<BufferContext*> buffers;
        boost::thread thread;
        boost::recursive_mutex mutex;
        pa_threaded_mainloop* pulseMainLoop;
        pa_context* pulseContext;
        pa_stream* pulseStream;
};
