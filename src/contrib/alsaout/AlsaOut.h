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
#pragma once

#include "pch.h"


#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>

#include <core/sdk/IOutput.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>
#include <list>

using namespace musik::core::audio;

class AlsaOut : public IOutput {
    public:
        AlsaOut();
        virtual ~AlsaOut();

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
            musik::core::audio::IBuffer *buffer;
            musik::core::audio::IBufferProvider *provider;
        };

        size_t CountBuffersWithProvider(IBufferProvider* provider);
        void SetFormat(musik::core::audio::IBuffer *buffer);
        void InitDevice();
        void CloseDevice();
        void WriteLoop();

        std::string device;
        snd_pcm_t* pcmHandle;
        snd_pcm_hw_params_t* hardware;
        snd_pcm_format_t pcmFormat;
        snd_pcm_access_t pcmType;

        size_t channels;
        size_t rate;
        double volume;
        volatile bool quit, initialized;

        std::unique_ptr<boost::thread> writeThread;
        boost::recursive_mutex stateMutex;
        boost::condition threadEvent;

        std::list<std::shared_ptr<BufferContext> > buffers;
        boost::mutex mutex;
};
