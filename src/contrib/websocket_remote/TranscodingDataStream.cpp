//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "TranscodingDataStream.h"
#include "Util.h"
#include <boost/filesystem.hpp>
#include <algorithm>

#define BUFFER_SIZE 8192
#define SAMPLES_PER_BUFFER BUFFER_SIZE / 4 /* sizeof(float) */

using PositionType = TranscodingDataStream::PositionType;

TranscodingDataStream::TranscodingDataStream(
    Context& context, const std::string& uri, size_t bitrate)
{
    this->input = nullptr;
    this->decoder = nullptr;
    this->pcmBuffer = nullptr;
    this->length = 0;
    this->position = 0;
    this->bitrate = bitrate;
    this->interrupted = false;
    this->eof = false;
    this->lame = nullptr;
    this->outFile = nullptr;

    this->input = context.environment->GetDataStream(uri.c_str());
    if (this->input) {
        this->decoder = context.environment->GetDecoder(this->input);
        if (this->decoder) {
            this->pcmBuffer = context.environment->GetBuffer(SAMPLES_PER_BUFFER);
            this->length = (PositionType)(this->decoder->GetDuration() * 1000.0 * (float)bitrate / 8.0);
        }
    }
}

TranscodingDataStream::TranscodingDataStream(
    Context& context,
    const std::string& uri,
    const std::string& tempFilename,
    const std::string& finalFilename,
    size_t bitrate)
: TranscodingDataStream(context, uri, bitrate)
{
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

TranscodingDataStream::~TranscodingDataStream() {
    this->Close();
}

bool TranscodingDataStream::Open(const char *uri, unsigned int options) {
    return true;
}

bool TranscodingDataStream::Close() {
    if (this->pcmBuffer) {
        this->pcmBuffer->Destroy();
        this->pcmBuffer = nullptr;
    }

    if (this->decoder != nullptr) {
        this->decoder->Destroy();
        this->decoder = nullptr;
    }

    if (this->input != nullptr) {
        this->input->Destroy();
        this->input = nullptr;
    }

    if (this->outFile) {
        fclose(this->outFile);
        this->outFile = nullptr;
        boost::system::error_code ec;
        boost::filesystem::remove(this->tempFilename, ec);
    }

    return true;
}

void TranscodingDataStream::Interrupt() {
    this->interrupted = true;
}

void TranscodingDataStream::Destroy() {
    delete this;
}

PositionType TranscodingDataStream::Read(void *buffer, PositionType bytesToRead) {
    if (this->eof || !this->pcmBuffer) {
        return 0;
    }

    size_t bytesWritten = 0;
    unsigned char* dst = (unsigned char*)buffer;
    bool hasBuffer = false;

    if (this->decoder && !this->lame) {
        hasBuffer = this->decoder->GetBuffer(this->pcmBuffer);
        if (hasBuffer) {
            this->lame = lame_init();
            lame_set_in_samplerate(this->lame, this->pcmBuffer->SampleRate());
            lame_set_VBR(lame, vbr_off);
            lame_set_VBR_mean_bitrate_kbps(lame, this->bitrate);
            lame_set_out_samplerate(lame, this->pcmBuffer->SampleRate());
            lame_init_params(lame);
        }
    }

    /* see if we have some stuff left over from last time through... */
    if (!spillover.empty()) {
        size_t count = std::min(spillover.avail(), (size_t) bytesToRead);
        memcpy(dst, spillover.pos(), count);

        if (this->outFile) {
            fwrite(spillover.pos(), 1, count, this->outFile);
        }

        spillover.inc(count);
        bytesWritten += count;
    }

    if (bytesWritten == bytesToRead) {
        this->position += bytesWritten;
        return bytesWritten; /* filled from the spillover... */
    }

    if (!hasBuffer) {
        hasBuffer = this->decoder->GetBuffer(this->pcmBuffer);
    }

    while (hasBuffer && bytesWritten < (size_t) bytesToRead) {
        /* calculated according to lame.h */
        size_t numSamples = pcmBuffer->Samples() / pcmBuffer->Channels();
        size_t requiredBytes = (size_t) (1.25 * (float)numSamples + 7200.0);

        encodedBytes.realloc(requiredBytes);

        /* encode PCM -> MP3 */
        int encodeCount =
            lame_encode_buffer_interleaved_ieee_float(
                lame,
                pcmBuffer->BufferPointer(),
                numSamples,
                encodedBytes.data,
                encodedBytes.length);

        if (encodeCount < 0) {
            goto internal_error;
        }

        encodedBytes.length = (size_t)encodeCount;

        /* if we got something, let's write it to the output buffer */
        if (encodedBytes.length) {
            size_t toWrite = std::min(
                encodedBytes.length,
                (size_t)(bytesToRead - bytesWritten));

            memcpy(dst + bytesWritten, encodedBytes.data, toWrite);

            if (this->outFile) {
                fwrite(encodedBytes.data, 1, toWrite, this->outFile);
            }

            encodedBytes.inc(toWrite);
            bytesWritten += toWrite;

            /* if we have decoded bytes still available, that means the
            output buffer is exhausted. swap it into the spillover buffer
            so it can be finalized the next time through. */
            if (encodedBytes.avail()) {
                spillover.swap(encodedBytes);
                this->position += bytesWritten;
                return bytesWritten;
            }
        }

        /* if we get here we still need to write some more data to
        the output buffer. get our next buffer of MP3 data and go
        through the loop again. */
        hasBuffer =
            ((size_t) bytesToRead > bytesWritten) &&
            this->decoder->GetBuffer(this->pcmBuffer);
    }

    /* finalize */
    if (bytesWritten == 0) {
        encodedBytes.reset();

        size_t count = lame_encode_flush(
            lame,
            encodedBytes.data,
            encodedBytes.length);

        memcpy(dst + bytesWritten, encodedBytes.data, count);

        if (this->outFile) {
            fwrite(encodedBytes.data, 1, count, this->outFile);
            fclose(this->outFile);
            this->outFile = nullptr;

            boost::system::error_code ec;
            boost::filesystem::rename(this->tempFilename, this->finalFilename, ec);
            if (ec) {
                boost::filesystem::remove(this->tempFilename, ec);
            }
        }

        this->eof = true;
    }

    this->position += bytesWritten;
    return bytesWritten;

internal_error:
    this->eof = true;
    fclose(this->outFile);
    this->outFile = nullptr;
    return 0;
}

bool TranscodingDataStream::SetPosition(PositionType position) {
    return false;
}

PositionType TranscodingDataStream::Position() {
    return this->position;
}

bool TranscodingDataStream::Seekable() {
    return false;
}

bool TranscodingDataStream::Eof() {
    return this->eof;
}

long TranscodingDataStream::Length() {
    return this->length;
}

const char* TranscodingDataStream::Type() {
    return "audio/mpeg";
}

const char* TranscodingDataStream::Uri() {
    if (this->input) {
        return this->input->Uri();
    }
    return "";
}

bool TranscodingDataStream::CanPrefetch() {
    return true;
}
