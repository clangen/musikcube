//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "InternalOpusDecoder.h"
#include <stdexcept>

InternalOpusDecoder::InternalOpusDecoder(int channels) :
		InternalDecoder(), channels(0), error(0), decoder(
				opus_decoder_create(48000, channels, &error))
{
	if (error != OPUS_OK)
		throw std::runtime_error(opus_strerror(error));
}

InternalOpusDecoder::~InternalOpusDecoder()
{
	opus_decoder_destroy(this->decoder);
}

int InternalOpusDecoder::DecodeData(std::vector<float> &outputBuffer,
		int channels, std::vector<std::uint8_t> data)
{
	int frameSize = opus_packet_get_samples_per_frame(data.data(), 48000);
	float buffer[frameSize * channels];
	int samples = opus_decode_float(this->decoder, data.data(), data.size(),
			buffer, frameSize, 0);
	if (samples >= 0) {
		outputBuffer.insert(outputBuffer.end(), buffer,
				buffer + (samples * channels));
	}
	return samples;
}
