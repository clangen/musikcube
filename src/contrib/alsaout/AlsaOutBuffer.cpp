//////////////////////////////////////////////////////////////////////////////
// Copyright ﺏ 2007, Daniel ﺿnnerby
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
#include "AlsaOutBuffer.h"
#include "AlsaOut.h"

//////////////////////////////////////////////////////////////////////////////

AlsaOutBuffer::AlsaOutBuffer(AlsaOut *alsaOut,IBuffer *buffer,IPlayer *player)
 :waveOut(alsaOut)
 ,buffer(buffer)
 ,player(player)
{
    this->PrepareBuffer();
}

void AlsaOutBuffer::PrepareBuffer(){
#ifdef _DEBUG
	std::cerr << "AlsaOutBuffer::PrepareBuffer()" << std::endl;
#endif
	//unsigned float bufSize = this->buffer->Samples()*this->buffer->Channels()*sizeof(float);
    /*// Prepare the header
    this->header.dwBufferLength		= this->buffer->Samples()*this->buffer->Channels()*sizeof(float);
    this->header.lpData				= (LPSTR)this->buffer->BufferPointer();
	this->header.dwUser				= (DWORD_PTR)this;
	this->header.dwBytesRecorded	= 0;
	this->header.dwFlags			= 0;
	this->header.dwLoops			= 0;
    this->header.lpNext             = NULL;
    this->header.reserved           = NULL;

    MMRESULT result = waveOutPrepareHeader(this->waveOut->waveHandle,&this->header,sizeof(WAVEHDR));
    if(result!=MMSYSERR_NOERROR){
        throw;
    }
    this->header.dwFlags			|= WHDR_DONE;
	*/
	data = this->buffer->BufferPointer();
	bufferLength = this->buffer->Samples()*this->buffer->Channels()*sizeof(float);
			//TODO: Check data types.

}

AlsaOutBuffer::~AlsaOutBuffer(void)
{
    this->player->ReleaseBuffer(this->buffer);
}

bool AlsaOutBuffer::AddToOutput(){
#ifdef _DEBUG
	std::cerr << "AlsaOutBuffer::AddToOutput()" << std::endl;
	std::cerr << "Waveout: " << waveOut << std::endl;
#endif
    /*MMRESULT result = waveOutWrite(this->waveOut->waveHandle,&this->header,sizeof(WAVEHDR));
    if(result==MMSYSERR_NOERROR){
        return true;
    }*/
	int err;
	snd_pcm_t* wHandle = waveOut->getWaveHandle();
#ifdef _DEBUG
	std::cerr << "whandle: " << wHandle << std::endl;
	std::cerr << "data: " << *data << std::endl;
	std::cerr << "bufferLength: " << bufferLength << std::endl;
#endif
	if (wHandle == NULL) {
		printf("Error. No device handle \n");
		return false;
	}
	frames = snd_pcm_writei(wHandle, (void*)data, bufferLength);
#ifdef _DEBUG
	std::cerr << "frames: " << frames << std::endl;
#endif
                 if (frames < 0)
                         frames = snd_pcm_recover(wHandle, frames, 0);
                 if (frames < 0) {
                         printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
                         return false;
                 }
                 if (frames > 0 && frames < (long)bufferLength)	{
                         printf("Short write (expected %li, wrote %li)\n", (long)bufferLength, frames);
                         return false;
                 }
	 else return true;
}

