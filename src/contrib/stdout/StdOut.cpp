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
#include "StdOut.h"

StdOut::StdOut()
 :waveHandle(NULL)
 ,maxBuffers(32)
 ,currentVolume(1.0)
 ,addToRemovedBuffers(false)
 ,device("default")
 ,output(NULL)
{
#ifdef _DEBUG
	std::cerr << "StdOut::StdOut() called" << std::endl;
#endif
}

StdOut::~StdOut(){
#ifdef _DEBUG
	std::cerr << "StdOut::~StdOut() called" << std::endl;
#endif
    this->ClearBuffers();
}

void* StdOut::getWaveHandle() {
#ifdef _DEBUG
	std::cerr << "StdOut::getWaveHandle() called" << std::endl;
#endif
	return this->waveHandle;
}

void StdOut::Destroy(){
#ifdef _DEBUG
	std::cerr << "StdOut::Destroy() called" << std::endl;
#endif
    delete this;
}

void StdOut::Pause(){
#ifdef _DEBUG
	std::cerr << "StdOut::Pause() called" << std::endl;
#endif
    //TODO: Do something here
}

void StdOut::Resume(){
#ifdef _DEBUG
	std::cerr << "StdOut::Resume() called" << std::endl;
#endif
    //TODO: Do something here
}

void StdOut::SetVolume(double volume){
#ifdef _DEBUG
	std::cerr << "StdOut::SetVolume() called" << std::endl;
#endif
    this->currentVolume = volume; //TODO: Finish SetVolume() function
}

void StdOut::ClearBuffers(){
#ifdef _DEBUG
	std::cerr << "StdOut::ClearBuffers() called" << std::endl;
#endif
    //TODO: Clear buffers
}

void StdOut::RemoveBuffer(StdOutBuffer *buffer){
#ifdef _DEBUG
	std::cerr << "StdOut::RemoveBuffer() called" << std::endl;
#endif
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

void StdOut::ReleaseBuffers(){
#ifdef _DEBUG
	std::cerr << "StdOut::ReleaseBuffers() called" << std::endl;
#endif
    BufferList clearBuffers;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        for(BufferList::iterator buf=this->removedBuffers.begin();buf!=this->removedBuffers.end();){
            clearBuffers.push_back(*buf);
            buf = this->removedBuffers.erase(buf);
        }
    }

}

bool StdOut::PlayBuffer(IBuffer *buffer,IPlayer *player){
#ifdef _DEBUG
	std::cerr << "StdOut::PlayBuffer() called" << std::endl;
#endif

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

        // Add to the waveout internal buffers
        StdOutBufferPtr outBuffer(new StdOutBuffer(this,buffer,player));

        // Header should now be prepared, lets add to waveout
        if( outBuffer->AddToOutput() ){
            // Add to the buffer list
            {
                boost::mutex::scoped_lock lock(this->mutex);
                this->buffers.push_back(outBuffer);
            }
            return true;
        }

    }

    return false;
}

void StdOut::SetFormat(IBuffer *buffer){
#ifdef _DEBUG
	std::cerr << "StdOut::SetFormat() called" << std::endl;
#endif
    if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->waveHandle==NULL){
        this->currentChannels   = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

        // Close old waveout
        if(this->waveHandle!=NULL){
            this->waveHandle    = NULL;
        }
/*
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
*/

        // Set the volume if it's not already set
        this->SetVolume(this->currentVolume);
    }
}
