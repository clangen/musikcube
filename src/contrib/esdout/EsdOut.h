//////////////////////////////////////////////////////////////////////////////
// Copyright © 2009, Julian Cromarty
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
#include "EsdOutBuffer.h"
/*
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>

#include <core/audio/IAudioCallback.h>
#include <core/audio/IAudioOutput.h>
*/
#include <core/audio/IOutput.h>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

using namespace musik::core::audio;

class EsdOut : public IOutput{
    public:
        EsdOut();
        ~EsdOut();

        virtual void Destroy();
        //virtual void Initialize(IPlayer *player);
        virtual void Pause();
        virtual void Resume();
        virtual void SetVolume(double volume);
        virtual void ClearBuffers();
        virtual bool PlayBuffer(IBuffer *buffer,IPlayer *player);
        virtual void ReleaseBuffers();
        
        int* getWaveHandle();

    public:
        typedef boost::shared_ptr<EsdOutBuffer> EsdOutBufferPtr;

        //static void CALLBACK WaveCallback(HWAVEOUT hWave, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2);
        void RemoveBuffer(EsdOutBuffer *buffer);

    private:
        void SetFormat(IBuffer *buffer);
        char *device;                        /* playback device */

    protected:
        friend class EsdOutBuffer;

        //IPlayer *player;

        // Audio stuff
        int *waveHandle;
        //snd_pcm_t        *waveHandle;
        esd_format_t waveFormat;
        //snd_pcm_access_t waveAccess;
        //snd_pcm_hw_params_t *hw_params;

        /*int bits = ESD_BITS16, channels = ESD_STEREO;
        int mode = ESD_STREAM, func = ESD_PLAY ;*/

        // Current format
        int currentBits;
        int currentChannels;
        long currentSampleRate;
        int currentMode;
        int currentFunc;
        double currentVolume;

        typedef std::list<EsdOutBufferPtr> BufferList;
        BufferList buffers;
        BufferList removedBuffers;
        size_t maxBuffers;

        boost::mutex mutex;

        bool addToRemovedBuffers;

};
