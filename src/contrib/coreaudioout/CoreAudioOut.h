#pragma once

#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>
#include <queue>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>

class CoreAudioOut : public musik::core::audio::IOutput {
    public:
        CoreAudioOut();
        virtual ~CoreAudioOut();

        virtual void Destroy();
        virtual void Pause();
        virtual void Resume();
        virtual void SetVolume(double volume);
        virtual void Stop();

        virtual bool Play(
            musik::core::audio::IBuffer *buffer,
            musik::core::audio::IBufferProvider *provider);

        void NotifyBufferCompleted(musik::core::audio::IBuffer *buffer);

    private:
        musik::core::audio::IBufferProvider *bufferProvider;
        AudioStreamBasicDescription audioFormat;
        AudioQueueRef audioQueue;
        size_t bufferCount;
        boost::thread thread;
        boost::recursive_mutex mutex;
        bool quit;
};
