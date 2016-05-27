#pragma once

#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>

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
};
