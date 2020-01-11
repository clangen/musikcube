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

#include "TranscodingAudioDataStream.h"
#include "Util.h"
#include <boost/filesystem.hpp>
#include <algorithm>

#define BUFFER_SIZE 8192
#define SAMPLES_PER_BUFFER BUFFER_SIZE / 4 /* sizeof(float) */

using PositionType = TranscodingAudioDataStream::PositionType;

TranscodingAudioDataStream::TranscodingAudioDataStream(
    Context& context,
    musik::core::sdk::IStreamingEncoder* encoder,
    const std::string& uri,
    size_t bitrate,
    const std::string& format)
: context(context)
{
    this->encoder = encoder;
    this->input = nullptr;
    this->decoder = nullptr;
    this->pcmBuffer = nullptr;
    this->length = 0;
    this->position = 0;
    this->bitrate = bitrate;
    this->interrupted = false;
    this->eof = false;
    this->encoder = nullptr;
    this->outFile = nullptr;
    this->detachTolerance = 0;
    this->format = format;

    this->input = context.environment->GetDataStream(uri.c_str(), OpenFlags::Read);
    if (this->input) {
        this->decoder = context.environment->GetDecoder(this->input);
        if (this->decoder) {
            this->pcmBuffer = context.environment->GetBuffer(SAMPLES_PER_BUFFER);

            /* note that we purposely under-estimate the content length by 1.0
            seconds; we do this because http clients seem to be more likely to be
            throw a fit if we over estimate, instead of under-estimate. */
            this->length = (PositionType)((this->decoder->GetDuration() - 1.0) * 1000.0 * (float)bitrate / 8.0);

            /* after the stream ends we allow decoding for up to an additional 5
            seconds to account for rounding errors in length estimation and decoder
            duration calculation. */
            this->detachTolerance = (PositionType)((5.0) * 1000.0 * (float)bitrate / 8.0);
        }
    }
}

TranscodingAudioDataStream::TranscodingAudioDataStream(
    Context& context,
    musik::core::sdk::IStreamingEncoder* encoder,
    const std::string& uri,
    const std::string& tempFilename,
    const std::string& finalFilename,
    size_t bitrate,
    const std::string& format)
: TranscodingAudioDataStream(context, encoder, uri, bitrate, format)
{
    this->encoder = encoder;
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

TranscodingAudioDataStream::~TranscodingAudioDataStream() {
}

bool TranscodingAudioDataStream::Open(const char *uri, OpenFlags flags) {
    return true;
}

bool TranscodingAudioDataStream::Close() {
    if (this->eof) {
        this->Dispose();
    }
    else {
        std::thread([this]() { /* detach and finish. hopefully. */
            char buffer[8192];
            int count = 0;
            int last = 0;
            while (!Eof() && count < detachTolerance) {
                last = Read(buffer, sizeof(buffer));
                count += last;
            }

            if (last != 0 && this->outFile) {
                /* incomplete, delete... */
                fclose(this->outFile);
                this->outFile = nullptr;
                boost::system::error_code ec;
                boost::filesystem::remove(this->tempFilename, ec);
            }

            Dispose();
        }).detach();
    }

    return true;
}

void TranscodingAudioDataStream::Dispose() {
    if (this->pcmBuffer) {
        this->pcmBuffer->Release();
        this->pcmBuffer = nullptr;
    }

    if (this->decoder) {
        this->decoder->Release();
        this->decoder = nullptr;
    }

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

void TranscodingAudioDataStream::Interrupt() {
    this->interrupted = true;
}

void TranscodingAudioDataStream::Release() {
    this->Dispose();
}

PositionType TranscodingAudioDataStream::Read(void *buffer, PositionType bytesToRead) {
    if (this->eof || !this->pcmBuffer) {
        return 0;
    }

    if (!this->encoder) {
        this->eof = true;
        return 0;
    }

    size_t bytesWritten = 0;
    char* dst = (char*) buffer;
    bool hasBuffer = false;

    /* init */
    if (this->decoder && !this->encoder) {
        hasBuffer = this->decoder->GetBuffer(this->pcmBuffer);
        if (hasBuffer) {
            this->encoder->Initialize(
                this->pcmBuffer->SampleRate(),
                this->pcmBuffer->Channels(),
                this->bitrate);
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
        this->position += (PositionType) bytesWritten;
        return (PositionType) bytesWritten; /* filled from the spillover... */
    }

    if (!hasBuffer) {
        hasBuffer = this->decoder->GetBuffer(this->pcmBuffer);
    }

    if (!hasBuffer && !decoder->Exhausted()) {
        goto internal_error;
    }

    /* done with spillover. read new data... */
    while (hasBuffer && bytesWritten < (size_t) bytesToRead) {
        char* encodedData = nullptr;
        int encodedLength = encoder->Encode(this->pcmBuffer, &encodedData);

        if (encodedLength < 0) {
            goto internal_error;
        }

        /* write to the output buffer */
        if (encodedLength > 0) {
            int toWrite = std::min(
                encodedLength,
                (int)(bytesToRead - bytesWritten));

            memcpy(dst + bytesWritten, encodedData, toWrite);

            if (this->outFile) {
                fwrite(encodedData, 1, toWrite, this->outFile);
            }

            bytesWritten += toWrite;

            /* if we have decoded bytes still available, that means the
            output buffer is exhausted. swap it into the spillover buffer
            so it can be finalized the next time through. */
            if (encodedLength > toWrite) {
                spillover.from(encodedData + toWrite, encodedLength - toWrite);
                this->position += (PositionType) bytesWritten;
                return (PositionType) bytesWritten;
            }
        }

        /* if we get here we still need to write some more data to
        the output buffer. get our next buffer of pcm data and go
        through the loop again. */
        hasBuffer =
            ((size_t) bytesToRead > bytesWritten) &&
            this->decoder->GetBuffer(this->pcmBuffer);
    }

    /* finalize */
    if (bytesWritten == 0) {
        char* encodedData = nullptr;
        int encodedLength = this->encoder->Flush(&encodedData);

        this->eof = true;

        if (encodedLength >= 0) {
            memcpy(dst + bytesWritten, encodedData, encodedLength);

            if (this->outFile) {
                fwrite(encodedData, 1, encodedLength, this->outFile);
                fclose(this->outFile);
                this->outFile = nullptr;

                this->encoder->Finalize(this->tempFilename.c_str());

                boost::system::error_code ec;
                boost::filesystem::rename(this->tempFilename, this->finalFilename, ec);
                if (ec) {
                    boost::filesystem::remove(this->tempFilename, ec);
                }
            }
        }
        else {
            goto internal_error;
        }
    }

    this->position += (PositionType) bytesWritten;
    return (PositionType) bytesWritten;

internal_error:
    this->eof = true;
    fclose(this->outFile);
    this->outFile = nullptr;
    boost::system::error_code ec;
    boost::filesystem::remove(this->tempFilename, ec);
    return 0;
}

bool TranscodingAudioDataStream::SetPosition(PositionType position) {
    return false;
}

PositionType TranscodingAudioDataStream::Position() {
    return this->position;
}

bool TranscodingAudioDataStream::Seekable() {
    return false;
}

bool TranscodingAudioDataStream::Eof() {
    return this->eof;
}

long TranscodingAudioDataStream::Length() {
    return this->length;
}

const char* TranscodingAudioDataStream::Type() {
    return "audio/mpeg";
}

const char* TranscodingAudioDataStream::Uri() {
    if (this->input) {
        return this->input->Uri();
    }
    return "";
}

bool TranscodingAudioDataStream::CanPrefetch() {
    return true;
}
