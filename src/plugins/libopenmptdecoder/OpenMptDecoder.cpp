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

#include "OpenMptDecoder.h"
#include "OpenMptDataStream.h"
#include <musikcore/sdk/IDebug.h>
#include <cassert>

using namespace musik::core::sdk;

extern IDebug* debug;

static const int kSampleRate = 48000;
static const int kSamplesPerChannel = 2048;
static const int kChannels = 2;

static size_t readCallback(void *user, void *dst, size_t bytes) {
    auto stream = static_cast<OpenMptDecoder*>(user)->Stream();
    return (size_t) stream->Read(dst, (PositionType) bytes);
}

static int seekCallback(void *user, int64_t offset, int whence) {
    auto stream = static_cast<OpenMptDecoder*>(user)->Stream();
    switch (whence) {
        case OPENMPT_STREAM_SEEK_SET:
            return stream->SetPosition((PositionType) offset) ? 0 : -1;
        case OPENMPT_STREAM_SEEK_CUR:
            return stream->SetPosition((PositionType) offset + stream->Position()) ? 0 : -1;
        case OPENMPT_STREAM_SEEK_END:
            return stream->SetPosition(stream->Length() - 1 - (PositionType)offset) ? 0 : -1;
    }
    return -1;
}

static int64_t tellCallback(void *user) {
    return (int64_t) static_cast<OpenMptDecoder*>(user)->Stream()->Position();
}

static void logCallback(const char *message, void *userdata) {
    if (debug) {
        debug->Info("OpenMtpDecoder", message);
    }
}

OpenMptDecoder::OpenMptDecoder() {
    this->module = nullptr;
}

OpenMptDecoder::~OpenMptDecoder() {
    if (this->module) {
        openmpt_module_destroy(this->module);
        this->module = nullptr;
    }

    if (this->isWrappedStream) {
        delete this->stream;
        this->stream = nullptr;
    }
}

bool OpenMptDecoder::Open(musik::core::sdk::IDataStream *stream) {
    auto mptStream = dynamic_cast<OpenMptDataStream*>(stream);
    if (mptStream) {
        this->stream = mptStream;
        this->isWrappedStream = false;
    }
    else {
        mptStream = new OpenMptDataStream(stream);
        if (!mptStream->Parse(stream->Uri())) {
            delete mptStream;
            mptStream = nullptr;
            return false;
        }
        this->stream = mptStream;
        this->isWrappedStream = true; /* we need to clean it up later */
    }

    openmpt_stream_callbacks callbacks = { 0 };
    callbacks.read = readCallback;
    callbacks.seek = seekCallback;
    callbacks.tell = tellCallback;

    this->module = openmpt_module_create2(
        callbacks, this, logCallback,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (this->module) {
        int track = mptStream->GetTrackNumber();
        if (track >= 0 && track < openmpt_module_get_num_subsongs(module)) {
            openmpt_module_select_subsong(this->module, track);
        }
        return true;
    }

    return false;
}

void OpenMptDecoder::Release() {
    delete this;
}

double OpenMptDecoder::SetPosition(double seconds) {
    if (this->module) {
        return openmpt_module_set_position_seconds(this->module, seconds);
    }
    return 0.0;
}

double OpenMptDecoder::GetDuration() {
    if (this->module) {
        return openmpt_module_get_duration_seconds(this->module);
    }
    return 0.0;
}

bool OpenMptDecoder::GetBuffer(IBuffer *target) {
    if (this->module) {
        target->SetSampleRate(kSampleRate);
        target->SetSamples(kSamplesPerChannel * kChannels);

        int samplesWritten = (int) openmpt_module_read_interleaved_float_stereo(
            this->module,
            (int32_t) kSampleRate,
            (size_t) kSamplesPerChannel,
            target->BufferPointer());

        if (samplesWritten > 0) {
            target->SetSamples(samplesWritten * kChannels);
            return true;
        }
    }

    return false;
}

bool OpenMptDecoder::Exhausted() {
    if (this->module) {
        return openmpt_module_get_position_seconds(this->module) >= this->GetDuration();
    }
    return true;
}
