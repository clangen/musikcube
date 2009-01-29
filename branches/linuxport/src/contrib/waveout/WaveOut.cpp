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
#include "WaveOut.h"

WaveOut::WaveOut()
 :waveHandle(NULL)
 ,maxBuffers(32)
 ,currentVolume(1.0)
 ,addToRemovedBuffers(false)
{
}

WaveOut::~WaveOut(){
    this->ClearBuffers();

    if(this->waveHandle!=NULL){
        waveOutClose(this->waveHandle);
        this->waveHandle    = NULL;
    }

}


void WaveOut::Destroy(){
    delete this;
}
/*
void WaveOut::Initialize(IPlayer *player){
    this->player    = player;
}
*/
void WaveOut::Pause(){
    waveOutPause(this->waveHandle);
}

void WaveOut::Resume(){
    waveOutRestart(this->waveHandle);
}

void WaveOut::SetVolume(double volume){
    if(this->waveHandle){
        DWORD newVolume = (DWORD)(volume*65535.0);
        newVolume   += newVolume*65536;

        waveOutSetVolume(this->waveHandle,newVolume); 
    }
    this->currentVolume = volume;
}

void WaveOut::ClearBuffers(){
    waveOutReset(this->waveHandle);
}

void WaveOut::RemoveBuffer(WaveOutBuffer *buffer){
    BufferList clearBuffers;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        bool found(false);
        for(BufferList::iterator buf=this->buffers.begin();buf!=this->buffers.end() && !found;){
            if(buf->get()==buffer){
//                if( !(*buf)->ReadyToRelease() ){
                    this->removedBuffers.push_back(*buf);
//                }
                clearBuffers.push_back(*buf);
                buf=this->buffers.erase(buf);
                found=true;
            }else{
                ++buf;
            }
        }
    }
}

void WaveOut::ReleaseBuffers(){
    BufferList clearBuffers;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        for(BufferList::iterator buf=this->removedBuffers.begin();buf!=this->removedBuffers.end();){
            clearBuffers.push_back(*buf);
            buf = this->removedBuffers.erase(buf);
        }
    }

}

bool WaveOut::PlayBuffer(IBuffer *buffer,IPlayer *player){

    size_t bufferSize  = 0;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        bufferSize  = this->buffers.size();
    }

    // if the format should change, wait for all buffers to be released
    if(bufferSize>0 && (this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate())){
        // Format has changed
//        this->player->Notify()
        return false;
    }


    if(bufferSize<this->maxBuffers){
        // Start by checking the format
        this->SetFormat(buffer);

        // Add to the waveout internal buffers
        WaveOutBufferPtr waveBuffer(new WaveOutBuffer(this,buffer,player));

        // Header should now be prepared, lets add to waveout
        if( waveBuffer->AddToOutput() ){
            // Add to the buffer list
            {
                boost::mutex::scoped_lock lock(this->mutex);
                this->buffers.push_back(waveBuffer);
            }
            return true;
        }

    }

    return false;
}

void WaveOut::SetFormat(IBuffer *buffer){
    if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->waveHandle==NULL){
        this->currentChannels   = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

        // Close old waveout
        if(this->waveHandle!=NULL){
            waveOutClose(this->waveHandle);
            this->waveHandle    = NULL;
        }

        // Create a new waveFormat
	    ZeroMemory(&this->waveFormat, sizeof(this->waveFormat));
        DWORD speakerconfig;

        // Set speaker configuration
        switch(buffer->Channels()){
            case 1:
                speakerconfig = KSAUDIO_SPEAKER_MONO;
                break;
            case 2:
                speakerconfig = KSAUDIO_SPEAKER_STEREO;
                break;
            case 4:
                speakerconfig = KSAUDIO_SPEAKER_QUAD;
                break;
            case 5:
                speakerconfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT);
                break;
            case 6:
                speakerconfig = KSAUDIO_SPEAKER_5POINT1;
                break;
            default:
                speakerconfig = 0;
        }

        this->waveFormat.Format.cbSize					= 22;
        this->waveFormat.Format.wFormatTag				= WAVE_FORMAT_EXTENSIBLE;
        this->waveFormat.Format.nChannels				= (WORD)buffer->Channels();
        this->waveFormat.Format.nSamplesPerSec			= (DWORD)buffer->SampleRate();
        this->waveFormat.Format.wBitsPerSample			= 32;
        this->waveFormat.Format.nBlockAlign			    = (this->waveFormat.Format.wBitsPerSample/8) * this->waveFormat.Format.nChannels;
        this->waveFormat.Format.nAvgBytesPerSec		    = ((this->waveFormat.Format.wBitsPerSample/8) * this->waveFormat.Format.nChannels) * this->waveFormat.Format.nSamplesPerSec; //Compute using nBlkAlign * nSamp/Sec 

        // clangen: wValidBitsPerSample/wReserved/wSamplesPerBlock are a union,
        // so don't set wReserved or wSamplesPerBlock to 0 after assigning
        // wValidBitsPerSample. (Vista bug)
        this->waveFormat.Samples.wValidBitsPerSample	= 32;
        this->waveFormat.dwChannelMask                  = speakerconfig; 
        this->waveFormat.SubFormat                      = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;


        // Create new waveout
        int openResult = waveOutOpen(
                &this->waveHandle, 
                WAVE_MAPPER, 
                (WAVEFORMATEX*)&this->waveFormat, 
                (DWORD_PTR)WaveCallback, 
                (DWORD_PTR)this, 
                CALLBACK_FUNCTION);

        if(openResult!=MMSYSERR_NOERROR){
            throw;
        }

        // Set the volume if it's not already set
        this->SetVolume(this->currentVolume);
    }
}

void CALLBACK WaveOut::WaveCallback(HWAVEOUT waveHandle, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2){
    if(msg==WOM_DONE){
        // A buffer is finished, lets release it
		LPWAVEHDR waveoutHeader         = (LPWAVEHDR)dw1;
        WaveOutBuffer *waveOutBuffer     = (WaveOutBuffer*)waveoutHeader->dwUser;

        waveOutBuffer->waveOut->RemoveBuffer(waveOutBuffer);
        waveOutBuffer->player->Notify();
    }
}
