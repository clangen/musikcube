//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, Daniel nnerby
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
#include <list>
#include <boost/shared_ptr.hpp> 
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include "WaveOutBuffer.h"
#include <core/sdk/IOutput.h>

using namespace musik::core::audio;

class WaveOut : public IOutput {
    public:
        WaveOut();
        ~WaveOut();

        virtual void Destroy();
        virtual void Pause();
        virtual void Resume();
        virtual void SetVolume(double volume);
        virtual void ClearBuffers();
        virtual bool PlayBuffer(IBuffer *buffer, IPlayer *player);
        virtual void ReleaseBuffers();

    public: 
        typedef boost::shared_ptr<WaveOutBuffer> WaveOutBufferPtr;
        static void CALLBACK WaveCallback(HWAVEOUT hWave, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2);

        void OnBufferWrittenToOutput(WaveOutBuffer *buffer);

    private:
        void SetFormat(IBuffer *buffer);

    protected:
        friend class WaveOutBuffer;
        typedef std::list<WaveOutBufferPtr> BufferList;

        HWAVEOUT waveHandle;
        WAVEFORMATPCMEX waveFormat;

        int currentChannels;
        long currentSampleRate;
        double currentVolume;

        BufferList buffers;
        size_t maxBuffers;

        boost::mutex mutex;
        boost::condition bufferRemovedCondition;
};
