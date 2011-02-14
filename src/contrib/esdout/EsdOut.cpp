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
{
	this->host = getenv("ESPEAKER");
	esd_server_info_t *info;
	if ((this->esd = esd_open_sound(this->host)) < 0)
	{
		std::cerr << "Couldn't connect to esd server" << std::endl;
	}
}

EsdOut::~EsdOut(){

    //esd_close( this->waveHandle );
    esd_close( this->esd );

}

int* EsdOut::getWaveHandle() {
	return &this->waveHandle;
}

void EsdOut::Destroy(){
    delete this;
}

void EsdOut::Pause(){
    esd_standby(this->esd);
}

void EsdOut::Resume(){
    esd_resume(this->esd);
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
    								//Will probably involve adjusting the data manually to be quieter
}

void EsdOut::ClearBuffers(){
	//TODO: check nothing needs doing here
}

void EsdOut::ReleaseBuffers(){
	//TODO: check nothing needs doing here
}

bool EsdOut::PlayBuffer(IBuffer *buffer,IPlayer *player){

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
        	//Don't think this is necessary as the esd server deals with its own buffers
            // Add to the buffer list
            /*{
                boost::mutex::scoped_lock lock(this->mutex);
                this->buffers.push_back(EsdBuffer);
            }*/
            return true;
        }

    }

    return false;
}

void EsdOut::SetFormat(IBuffer *buffer){

    if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->waveHandle==NULL){
        this->currentChannels   = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

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

    	this->waveHandle = esd_play_stream(this->waveFormat, (int)this->currentSampleRate, this->host, "musik");

    }
}
