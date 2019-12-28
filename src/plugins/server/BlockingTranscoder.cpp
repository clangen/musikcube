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
    IDataStreamEncoder* encoder,
    const std::string& uri,
    const std::string& tempFilename,
    const std::string& finalFilename)
: context(context) {
    this->input = nullptr;
    this->interrupted = false;
    this->encoder = nullptr;
    this->outFile = nullptr;
    this->input = context.environment->GetDataStream(uri.c_str());
    this->tempFilename = tempFilename;
    this->finalFilename = finalFilename;
    if (tempFilename.size() && finalFilename.size()) {
#ifdef WIN32
        this->outFile = _wfopen(utf8to16(tempFilename.c_str()).c_str(), L"wb");
#else
        this->outFile = fopen(tempFilename.c_str(), "wb");
#endif
    }
}

BlockingTranscoder::~BlockingTranscoder() {
    if (this->input) {
        this->input->Release();
        this->input = nullptr;
    }
    if (this->encoder) {
        this->encoder->Release();
        this->encoder = nullptr;
    }
    if (this->outFile) {
        fclose(this->outFile);
        this->outFile = nullptr;
        boost::system::error_code ec;
        boost::filesystem::remove(this->tempFilename, ec);
    }
    delete this;
}

bool BlockingTranscoder::Transcode() {
    if (!this->input || !this->encoder) {
        return false;
    }

    IDecoder* decoder = this->context.environment->GetDecoder(this->input);

    if (!decoder) {
        return false;
    }

    IBuffer* pcmBuffer = this->context.environment->GetBuffer(SAMPLES_PER_BUFFER);

    while (decoder->GetBuffer(pcmBuffer)) {
        this->encoder->Encode(pcmBuffer);
        std::this_thread::yield();
    }

    bool result = false;

    if (decoder->Exhausted()) {
        this->encoder->Finalize();
        fclose(this->outFile);
        this->outFile = nullptr;
        boost::system::error_code ec;
        boost::filesystem::rename(this->tempFilename, this->finalFilename, ec);
        if (ec) {
            boost::filesystem::remove(this->tempFilename, ec);
        }
        else {
            result = true;
        }
    }

    decoder->Release();
    pcmBuffer->Release();

    return result;
}

void BlockingTranscoder::Interrupt() {
    this->interrupted = true;
}
