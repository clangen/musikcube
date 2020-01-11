//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "BlockingTranscoder.h"
#include "Util.h"
#include <boost/filesystem.hpp>
#include <algorithm>

#define BUFFER_SIZE 8192
#define SAMPLES_PER_BUFFER BUFFER_SIZE / 4 /* sizeof(float) */

using namespace musik::core::sdk;

BlockingTranscoder::BlockingTranscoder(
    Context& context,
    IBlockingEncoder* encoder,
    const std::string& uri,
    const std::string& tempFilename,
    const std::string& finalFilename,
    int bitrate)
: context(context) {
    this->interrupted = false;
    this->bitrate = bitrate;
    this->encoder = encoder;
    this->tempFilename = tempFilename;
    this->finalFilename = finalFilename;
    this->output = context.environment->GetDataStream(tempFilename.c_str(), OpenFlags::Write);
    this->input = context.environment->GetDataStream(uri.c_str(), OpenFlags::Read);
}

BlockingTranscoder::~BlockingTranscoder() {
    this->Cleanup();
}

void BlockingTranscoder::Cleanup() {
    if (this->input) {
        this->input->Release();
        this->input = nullptr;
    }
    if (this->encoder) {
        this->encoder->Release();
        this->encoder = nullptr;
    }
    if (this->output) {
        this->output->Release();
        this->output = nullptr;
    }
}

bool BlockingTranscoder::Transcode() {
    if (!this->input || !this->output || !this->encoder) {
        return false;
    }

    IDecoder* decoder = this->context.environment->GetDecoder(this->input);

    if (!decoder) {
        return false;
    }

    IBuffer* pcmBuffer = this->context.environment->GetBuffer(SAMPLES_PER_BUFFER);

    bool result = false;

    if (decoder->GetBuffer(pcmBuffer)) {
        bool initialized = encoder->Initialize(
            this->output,
            pcmBuffer->SampleRate(),
            pcmBuffer->Channels(),
            this->bitrate);

        if (initialized) {
            this->encoder->Encode(pcmBuffer);
            while (!interrupted && decoder->GetBuffer(pcmBuffer)) {
                this->encoder->Encode(pcmBuffer);
                std::this_thread::yield();
            }

            if (decoder->Exhausted()) {
                this->encoder->Finalize();
                this->output->Release();
                this->output = nullptr;
                boost::system::error_code ec;
                boost::filesystem::rename(this->tempFilename, this->finalFilename, ec);
                if (ec) {
                    boost::filesystem::remove(this->tempFilename, ec);
                }
                else {
                    result = true;
                }
            }
        }
    }

    decoder->Release();
    pcmBuffer->Release();

    this->Cleanup();

    if (!result) {
        boost::system::error_code ec;
        boost::filesystem::remove(this->tempFilename, ec);
    }

    return result;
}

void BlockingTranscoder::Interrupt() {
    this->interrupted = true;
}
