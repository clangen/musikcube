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

#include "AlsaOut.h"

#define BUFFER_COUNT 8

using namespace musik::core::audio;

AlsaOut::AlsaOut()
: pcmHandle(NULL)
, currentVolume(1.0)
, device("default")
, output(NULL) {
#ifdef _DEBUG
	std::cerr << "AlsaOut::AlsaOut() called" << std::endl;
#endif
	int err;
	if ((err = snd_pcm_open (&pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		std::cerr << "AlsaOut: cannot open audio device 'default' :" << snd_strerror(err) << std::endl;
		return;
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio device opened" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_malloc (&hardware)) < 0) {
		std::cerr << "AlsaOut: cannot allocate hardware parameter structure " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_any (pcmHandle, hardware)) < 0) {
		std::cerr << "AlsaOut: cannot initialize hardware parameter structure " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_access (pcmHandle, hardware, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		std::cerr << "AlsaOut: cannot set access type " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: access type set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_format (pcmHandle, hardware, SND_PCM_FORMAT_S16_LE)) < 0) {
		std::cerr << "AlsaOut: cannot set sample format " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: sample format set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_rate_resample (pcmHandle, hardware, 44100)) < 0) {
		std::cerr << "AlsaOut: cannot set sample rate " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: sample rate set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_channels (pcmHandle, hardware, 2)) < 0) {
		std::cerr << "AlsaOut: cannot set channel count " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: channel count set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params (pcmHandle, hardware)) < 0) {
		std::cerr << "AlsaOut: cannot set parameters " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: parameters set" << std::endl;
	}
	#endif
	
	snd_pcm_hw_params_free (hardware);
	
	if ((err = snd_pcm_prepare (pcmHandle)) < 0) {
		std::cerr << "cannot prepare audio interface for use " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	std::cerr << "pcmHandle: " << pcmHandle << std::endl;
	#endif
}

AlsaOut::~AlsaOut() {
    if (this->pcmHandle){
        snd_pcm_close(this->pcmHandle);
        this->pcmHandle = NULL;
    }
}

void AlsaOut::Destroy() {
    delete this;
}

void AlsaOut::Stop() {
    snd_pcm_drop(this->pcmHandle);
    snd_pcm_reset(this->pcmHandle);
}

void AlsaOut::Pause() {
    snd_pcm_pause(this->pcmHandle, 1);
}

void AlsaOut::Resume() {
    snd_pcm_pause(this->pcmHandle, 0);
}

void AlsaOut::SetVolume(double volume) {

}

// void AlsaOut::RemoveBuffer(AlsaOutBuffer *buffer){
//     BufferList clearBuffers;
//     {
//         boost::mutex::scoped_lock lock(this->mutex);
//         bool found(false);
//         for(BufferList::iterator buf=this->buffers.begin();buf!=this->buffers.end() && !found;){
//             if(buf->get()==buffer){
//                 this->removedBuffers.push_back(*buf);
//                 clearBuffers.push_back(*buf);
//                 buf=this->buffers.erase(buf);
//                 found=true;
//             }else{
//                 ++buf;
//             }
//         }
//     }
// }

// void AlsaOut::ReleaseBuffers(){
//     BufferList clearBuffers;
//     {
//         boost::mutex::scoped_lock lock(this->mutex);
//         for(BufferList::iterator buf=this->removedBuffers.begin();buf!=this->removedBuffers.end();){
//             clearBuffers.push_back(*buf);
//             buf = this->removedBuffers.erase(buf);
//         }
//     }
// }

bool AlsaOut::Play(IBuffer *buffer, IBufferProvider*player) {
    return false;
// #ifdef _DEBUG
// 	std::cerr << "AlsaOut::PlayBuffer()" << std::endl;
// 	std::cerr << "Buffer: " << buffer << std::endl;
// #endif
//     size_t bufferSize  = 0;
//     {
//         boost::mutex::scoped_lock lock(this->mutex);
//         bufferSize  = this->buffers.size();
//     }
// #ifdef _DEBUG
// 	std::cerr << "buffer size: " << bufferSize << std::endl;
// #endif

//     // if the format should change, wait for all buffers to be released
//     if(bufferSize>0 && (this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate())){
// #ifdef _DEBUG
// 	std::cerr << "format changed" << std::endl;
// #endif
//         return false;
//     }


//     if(bufferSize<this->maxBuffers){
//         // Start by checking the format
//         this->SetFormat(buffer);
// #ifdef _DEBUG
// 	std::cerr << "format set" << std::endl;
// #endif

//         // Add to the waveout internal buffers
//         AlsaOutBufferPtr alsaBuffer(new AlsaOutBuffer(this,buffer,player));
// #ifdef _DEBUG
// 	std::cerr << "alsaBuffer created" << std::endl;
// #endif

//         // Header should now be prepared, lets add to waveout
//         if( alsaBuffer->AddToOutput() ){
// #ifdef _DEBUG
// 	std::cerr << "added to internal buffer" << std::endl;
// #endif
//             // Add to the buffer list
//             {
//                 boost::mutex::scoped_lock lock(this->mutex);
//                 this->buffers.push_back(alsaBuffer);
// #ifdef _DEBUG
// 	std::cerr << "added to buffer list" << std::endl;
// #endif
//             }
//             return true;
//         }

//     }
// #ifdef _DEBUG
// 	std::cerr << "buffer size too big" << std::endl;
// #endif

//     return false;
}

void AlsaOut::SetFormat(IBuffer *buffer){
  //   if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->pcmHandle==NULL){
  //       this->currentChannels   = buffer->Channels();
  //       this->currentSampleRate = buffer->SampleRate();

  //       // Close old waveout
  //       if(this->pcmHandle!=NULL){
  //           snd_pcm_close(this->pcmHandle);
  //           this->pcmHandle    = NULL;
  //       }
  //       this->pcmFormat						= SND_PCM_FORMAT_FLOAT_LE;
  //       this->pcmType						= SND_PCM_ACCESS_RW_INTERLEAVED;

  //       int err;
		// //Open the device
		// 	if ((err = snd_pcm_open(&this->pcmHandle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	 //         printf("Playback open error: %s\n", snd_strerror(err));
	 //         return;
  //        }
		// //Set simple parameters
		// 	if (( err = snd_pcm_set_params(
		// 			this->pcmHandle,
		// 			this->pcmFormat,
		// 			this->pcmType,
		// 			this->currentChannels,
		// 			this->currentSampleRate,
		// 			1,		//Allow alsa-lib software resampling
		// 			500000)	//Required overall latency (us) /* 0.5s */
		// 			) > 0)	{	//If an error...
		// 				printf("Playback open error: %s\n", snd_strerror(err));
	 //               exit(EXIT_FAILURE);
	 //            }

  //       // Set the volume if it's not already set
  //       this->SetVolume(this->currentVolume);
  //   }
}



// bool AlsaOutBuffer::AddToOutput(){
// #ifdef _DEBUG
//     std::cerr << "AlsaOutBuffer::AddToOutput()" << std::endl;
//     std::cerr << "Waveout: " << waveOut << std::endl;
// #endif
//     /*MMRESULT result = waveOutWrite(this->waveOut->pcmHandle,&this->header,sizeof(WAVEHDR));
//     if(result==MMSYSERR_NOERROR){
//         return true;
//     }*/
//     int err;
//     snd_pcm_t* wHandle = waveOut->getpcmHandle();
// #ifdef _DEBUG
//     std::cerr << "whandle: " << wHandle << std::endl;
//     std::cerr << "data: " << *data << std::endl;
//     std::cerr << "bufferLength: " << bufferLength << std::endl;
// #endif
//     if (wHandle == NULL) {
//         printf("Error. No device handle \n");
//         return false;
//     }
//     frames = snd_pcm_writei(wHandle, (void*)data, bufferLength);
// #ifdef _DEBUG
//     std::cerr << "frames: " << frames << std::endl;
// #endif
//                  if (frames < 0)
//                          frames = snd_pcm_recover(wHandle, frames, 0);
//                  if (frames < 0) {
//                          printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
//                          return false;
//                  }
//                  if (frames > 0 && frames < (long)bufferLength) {
//                          printf("Short write (expected %li, wrote %li)\n", (long)bufferLength, frames);
//                          return false;
//                  }
//      else return true;
// }
