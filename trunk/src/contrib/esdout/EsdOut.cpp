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
#include "EsdOut.h"

EsdOut::EsdOut()
 :waveHandle(NULL)
 ,maxBuffers(32)
 ,currentVolume(1.0)
 ,currentBits(16)
 ,addToRemovedBuffers(false)
 ,host(NULL)
{
#ifdef _DEBUG
	std::cerr << "EsdOut::EsdOut() called" << std::endl;
#endif
	//const char* host;
	/*int esd;
	esd_server_info_t *info;
	if ((esd = esd_open_sound(NULL)) >= 0)
	{
		info = esd_get_server_info(esd);
		esd_rate = info->rate;
		fmt = info->format;
		esd_free_server_info(info);
		esd_close(esd);
	}*/
}

EsdOut::~EsdOut(){
#ifdef _DEBUG
	std::cerr << "EsdOut::~EsdOut()" << std::endl;
#endif
    this->ClearBuffers();

    esd_close( this->waveHandle );

}

int* EsdOut::getWaveHandle() {
#ifdef _DEBUG
	std::cerr << "EsdOut::getWaveHandle()" << std::endl;
#endif
	return &this->waveHandle;
}

void EsdOut::Destroy(){
#ifdef _DEBUG
	std::cerr << "EsdOut::Destroy()" << std::endl;
#endif
    delete this;
}

void EsdOut::Pause(){
#ifdef _DEBUG
	std::cerr << "EsdOut::Pause()" << std::endl;
#endif
    esd_standby(this->waveHandle);
}

void EsdOut::Resume(){
#ifdef _DEBUG
	std::cerr << "EsdOut::Resume()" << std::endl;
#endif
    esd_resume(this->waveHandle);
}

void EsdOut::SetVolume(double volume){
#ifdef _DEBUG
	std::cerr << "EsdOut::SetVolume()" << std::endl;
#endif
    /*if(this->waveHandle){
        DWORD newVolume = (DWORD)(volume*65535.0);
        newVolume   += newVolume*65536;

        waveOutSetVolume(this->waveHandle,newVolume);
    }*/
    this->currentVolume = volume; //TODO: Write Esd SetVolume() function
}

void EsdOut::ClearBuffers(){
#ifdef _DEBUG
	std::cerr << "EsdOut::ClearBuffers()" << std::endl;
#endif
	 //snd_pcm_drop(this->waveHandle);
    //snd_pcm_reset(this->waveHandle);
	//TODO: check nothing needs doing here
}

void EsdOut::RemoveBuffer(EsdOutBuffer *buffer){
#ifdef _DEBUG
	std::cerr << "EsdOut::RemoveBuffer()" << std::endl;
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

void EsdOut::ReleaseBuffers(){
#ifdef _DEBUG
	std::cerr << "EsdOut::ReleaseBuffers()" << std::endl;
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

bool EsdOut::PlayBuffer(IBuffer *buffer,IPlayer *player){
#ifdef _DEBUG
	std::cerr << "EsdOut::PlayBuffer()" << std::endl;
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
        // Start by checking the format
        this->SetFormat(buffer);

        // Add to the waveout internal buffers
        EsdOutBufferPtr EsdBuffer(new EsdOutBuffer(this,buffer,player));

        // Header should now be prepared, lets add to waveout
        if( EsdBuffer->AddToOutput() ){
            // Add to the buffer list
            {
                boost::mutex::scoped_lock lock(this->mutex);
                this->buffers.push_back(EsdBuffer);
            }
            return true;
        }

    }

    return false;
}

void EsdOut::SetFormat(IBuffer *buffer){
#ifdef _DEBUG
	std::cerr << "EsdOut::SetFormat()" << std::endl;
#endif
    if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->waveHandle==NULL){
        this->currentChannels   = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();
#ifdef _DEBUG
      std::cerr << "Channels: " << this->currentChannels << std::endl;
      std::cerr << "Rate: " << this->currentSampleRate << std::endl;
      std::cerr << "Bits: " << this->currentBits << std::endl;
#endif
        if (this->waveHandle) {
        	esd_close(this->waveHandle);
        }

    	this->waveFormat = 0;

    	if (this->currentBits == 8) {
    		this->waveFormat |= ESD_BITS8;
    	}
    	else if (this->currentBits == 16) {
    		this->waveFormat |= ESD_BITS16;
    	}
    	else {
    		std::cerr << "EsdOut - Incorrect number of bits: " << this->currentBits << ". Should be 8 or 16" << std::endl;
    	}

    	if (this->currentChannels == 2) {
    		this->waveFormat |= ESD_STEREO;
    	}
    	else if (this->currentChannels == 1) {
    		this->waveFormat |= ESD_MONO;
    	}
    	else {
    		std::cerr << "EsdOut - Incorrect number of channels: " << this->currentChannels << ". Should be 1 or 2" << std::endl;
    	}

    	this->waveFormat |= ESD_STREAM;
    	this->waveFormat |= ESD_PLAY;

    	this->waveHandle = esd_play_stream(this->waveFormat, (int)this->currentSampleRate, "192.168.10.1:16001", "musik");
#ifdef _DEBUG
    	std::cerr << "waveHandle: " << this->waveHandle << std::endl;
#endif
    }
}
