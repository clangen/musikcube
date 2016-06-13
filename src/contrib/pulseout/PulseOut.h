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

#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>
#include <deque>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <pulse/pulseaudio.h>

class PulseOut : public musik::core::audio::IOutput {
    public:
        struct BufferContext {
            PulseOut *output;
            musik::core::audio::IBuffer *buffer;
            musik::core::audio::IBufferProvider *provider;
            unsigned long long lastByte;
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

    private:
        struct Pulse {
            pa_threaded_mainloop* loop;
            pa_stream* stream;
            pa_context* context;
            pa_sample_spec format;
        };

        static void OnPulseContextStateChanged(pa_context *c, void *data);
        static void OnPulseStreamStateChanged(pa_stream *s, void *data);
        static void OnPulseStreamSuccess(pa_stream *s, int success, void *data);
        static void OnPulseBufferPlayed(void *data);
        static void OnPulseStreamWrite(pa_stream *s, size_t bytes, void *data);

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
        Pulse pulse;
        unsigned long long bytesWritten;
        unsigned long long bytesConsumed;
        volatile bool quit;
};
