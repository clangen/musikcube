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
#include "StdOutBuffer.h"
#include "StdOut.h"

//////////////////////////////////////////////////////////////////////////////

StdOutBuffer::StdOutBuffer(StdOut *stdOut,IBuffer *buffer,IPlayer *player)
 :waveOut(stdOut)
 ,buffer(buffer)
 ,player(player)
{
#ifdef _DEBUG
	std::cerr << "StdOutBuffer::StdOutBuffer()" << std::endl;
#endif
	this->PrepareBuffer();
}

void StdOutBuffer::PrepareBuffer(){
#ifdef _DEBUG
	std::cerr << "StdOutBuffer::PrepareBuffer()" << std::endl;
#endif
}

StdOutBuffer::~StdOutBuffer(void)
{
#ifdef _DEBUG
	std::cerr << "StdOutBuffer::~StdOutBuffer()" << std::endl;
#endif
}

bool StdOutBuffer::AddToOutput(){
#ifdef _DEBUG
	std::cerr << "StdOutBuffer::AddToOutput()" << std::endl;
#endif
	void* wHandle = waveOut->getWaveHandle();
	if (wHandle == NULL) {
		printf("Error. No device handle \n");
		return false;
	}
	wHandle << (void*)data;
	
	return true;
}

