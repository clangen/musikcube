//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "pch.hpp"

#include "Stream.h"
#include <musikcore/audio/PlaybackMath.h>
#include "Dsp.h"
#include <musikcore/plugin/Plugins.h>
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <climits>
#include <musikcore/debug.h>

using namespace musik::core::audio;
using namespace musik::core::sdk;
namespace scratchBuffers = musik::core::ffi::audio_scratch_buffers;

static std::string TAG = "Stream";

static constexpr int MinimumBufferCount = 30;

Stream::Stream(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options)
: decoderSampleRate(0)
, decoderChannels(0)
, decoderSampleOffset(0)
, decoderSamplesRemain(0)
, decoderPosition(0)
, options(options)
, samplesPerChannel(samplesPerChannel)
, bufferCount(0)
, done(false)
, bufferLengthSeconds(bufferLengthSeconds)
, capabilities(0)
, rawBuffer(nullptr) {
    if (((int) this->options & (int) StreamFlags::NoDSP) == 0) {
        dsps = musik::core::plugin::CreateDsps();
    }
}

Stream::~Stream() {
    delete[] rawBuffer;
    if (this->decoder.instance) {
        this->decoder.api->destroy(this->decoder.instance);
    }
    if (this->dataStream.instance) {
        this->dataStream.api->destroy(this->dataStream.instance);
    }
    const auto& buffers = scratchBuffers::Host();
    buffers.destroy(buffers.context, &this->scratch);

    for (Buffer* buffer : this->recycledBuffers) {
        delete buffer;
    }

    for (Buffer* buffer : this->filledBuffers) {
        delete buffer;
    }
}

Stream::Shared Stream::Create(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options) {
    return Stream::Shared(new Stream(samplesPerChannel, bufferLengthSeconds, options));
}

musik::core::audio::IStream* Stream::CreateUnmanaged(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options) {
    return new Stream(samplesPerChannel, bufferLengthSeconds, options);
}

double Stream::SetPosition(double requestedSeconds) {
    double actualSeconds = -1;
    if (!this->decoder.instance ||
        this->decoder.api->seek(this->decoder.instance, requestedSeconds, &actualSeconds, nullptr) != MC_OK ||
        !std::isfinite(actualSeconds) || actualSeconds < 0)
    {
        return -1;
    }
    const auto samplePosition = actualSeconds * this->decoderSampleRate * this->decoderChannels;
    if (samplePosition >= static_cast<double>(UINT64_MAX)) {
        return -1;
    }
    this->decoderPosition = static_cast<uint64_t>(samplePosition);

    /* a successful seek invalidates the old decoder remainder and EOF,
    as well as the slices already waiting in the playback pool. */
    this->decoderSamplesRemain = 0;
    this->decoderSampleOffset = 0;
    this->decoderEnded = false;
    this->decodeStatus = MC_OK;
    this->done = false;

    auto it = this->filledBuffers.begin();
    while (it != this->filledBuffers.end()) {
        this->recycledBuffers.push_back(*it);
        ++it;
    }

    this->filledBuffers.clear();

    return actualSeconds;
}

double Stream::GetDuration() {
    double seconds;
    if (this->decoder.instance && this->decoder.api->get_duration(this->decoder.instance, &seconds) == MC_OK &&
        std::isfinite(seconds) && seconds >= 0)
    {
        return seconds;
    }
    return -1;
}

int Stream::GetCapabilities() {
    return this->capabilities;
}

bool Stream::GetInitialFormat(mc_audio_format& format) const {
    format = MC_AUDIO_FORMAT_DEFAULT;
    return this->decoder.instance &&
        this->decoder.api->struct_size >= offsetof(mc_decoder_vtable, get_format) + sizeof(this->decoder.api->get_format) &&
        this->decoder.api->get_format &&
        this->decoder.api->get_format(this->decoder.instance, &format) == MC_OK &&
        format.sample_rate > 0 && format.channels > 0;
}

bool Stream::OpenStream(std::string uri, Output* output) {
    const auto& media = musik::core::plugin::Media();
    const mc_string_view path { uri.data(), uri.size() };
    const auto failed = [](const char* operation, mc_result status) {
        if (status != MC_ERROR_CANCELLED) {
            musik::debug::error(TAG, std::string(operation) + " failed (status " + std::to_string(status) + ")");
        }
        return false;
    };
    mc_stream source = MC_STREAM_DEFAULT;
    auto status = media.create_stream(media.context, path, &source, nullptr);
    if (status != MC_OK) {
        return failed("stream creation", status);
    }

    /* publish each handle before calling into the provider. Stop can now wake
    a blocked open/read. the worker owns both handles until Stream destruction. */
    {
        std::lock_guard lock(this->sourceMutex);
        this->dataStream = source;
        if (this->sourceInterrupted) {
            return false;
        }
    }
    status = source.api->open(source.instance, path, MC_STREAM_OPEN_READ, nullptr);
    if (status != MC_OK) {
        return failed("stream open", status);
    }
    mc_decoder codec = MC_DECODER_DEFAULT;
    status = media.create_decoder(media.context, &this->dataStream, &codec, nullptr);
    if (status != MC_OK) {
        return failed("decoder creation", status);
    }
    {
        std::lock_guard lock(this->sourceMutex);
        this->decoder = codec;
        if (this->sourceInterrupted) {
            return false;
        }
    }
    status = codec.api->open(codec.instance, &this->dataStream, nullptr);
    if (status != MC_OK) {
        return failed("decoder open", status);
    }

    const auto rate = output ? output->GetDefaultSampleRate() : 0;
    if (rate > 0) {
        codec.api->set_preferred_sample_rate(codec.instance, rate);
    }
    const auto& buffers = scratchBuffers::Host();
    /* scratch starts empty. the decoder supplies the track format when it
    publishes its first block. */
    const mc_audio_format initial {
        .struct_size = sizeof(mc_audio_format),
        .sample_rate = 44100,
        .channels = 2,
        .channel_layout = 0,
    };
    status = buffers.create(buffers.context, &initial, 0, &this->scratch, nullptr);
    if (status != MC_OK) {
        return failed("decoder buffer creation", status);
    }
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    status = source.api->get_info(source.instance, &info);
    if (status != MC_OK) {
        return failed("stream information", status);
    }
    if (info.flags & MC_STREAM_CAN_PREFETCH) {
        this->capabilities |= (int) musik::core::sdk::Capability::Prebuffer;
        this->RefillInternalBuffers();
    }
    if (this->decodeStatus != MC_OK) {
        return failed("initial decode", this->decodeStatus);
    }
    return true;
}

void Stream::Interrupt() {
    mc_stream source;
    mc_decoder codec;
    {
        std::lock_guard lock(this->sourceMutex);
        if (this->sourceInterrupted) {
            return;
        }
        this->sourceInterrupted = true;
        source = this->dataStream;
        codec = this->decoder;
    }
    if (source.instance) {
        source.api->interrupt(source.instance);
    }
    if (codec.instance) {
        codec.api->interrupt(codec.instance);
    }
}

void Stream::RecycleBuffer(IBuffer* buffer) {
    this->recycledBuffers.push_back((Buffer*) buffer);
}

bool Stream::GetNextBufferFromDecoder() {
    if (this->decoderEnded) {
        return false;
    }
    mc_decode_state state = MC_DECODE_MORE;
    this->decodeStatus = this->decoder.api->decode(this->decoder.instance, this->scratch, &state, nullptr);
    if (this->decodeStatus == MC_OK) {
        const auto& buffers = scratchBuffers::Host();
        this->decodeStatus = buffers.read_block(buffers.context, this->scratch, &this->decoded);
    }
    if (this->decodeStatus != MC_OK) {
        return false;
    }
    this->decoderEnded = state == MC_DECODE_END;
    if (this->decoderEnded && !this->decoded.frame_count) {
        return false;
    }
    const auto& pcm = this->decoded;
    if ((state != MC_DECODE_MORE && state != MC_DECODE_END) || !pcm.data || !pcm.frame_count ||
        !pcm.format.channels || pcm.format.channels > INT_MAX / this->samplesPerChannel ||
        !pcm.format.sample_rate || pcm.format.sample_rate > INT_MAX ||
        pcm.frame_count > static_cast<size_t>(LONG_MAX) / pcm.format.channels)
    {
        this->decodeStatus = MC_ERROR_PLUGIN;
        musik::debug::error(TAG, "decoder returned an invalid PCM block");
        return false;
    }
    if (this->rawBuffer &&
        (this->decoderChannels != pcm.format.channels || this->decoderSampleRate != pcm.format.sample_rate))
    {
        this->decodeStatus = MC_ERROR_PLUGIN;
        musik::debug::error(TAG, "decoder changed PCM format during playback");
        return false;
    }

    if (!this->rawBuffer) {
        this->decoderSampleRate = pcm.format.sample_rate;
        this->decoderChannels = pcm.format.channels;
        this->samplesPerBuffer = samplesPerChannel * decoderChannels;

        this->bufferCount = pcmBufferCount(this->bufferLengthSeconds, this->decoderSampleRate,
            this->samplesPerChannel, MinimumBufferCount);

        /* allocate playback storage once and recycle fixed slices. decoder
        scratch storage is separate; output releases each slice before reuse. */
        this->rawBuffer = new float[static_cast<size_t>(bufferCount) * this->samplesPerBuffer];
        size_t offset = 0;
        for (int i = 0; i < bufferCount; i++) {
            auto buffer = new Buffer(this->rawBuffer + offset, this->samplesPerBuffer);
            buffer->SetSampleRate(this->decoderSampleRate);
            buffer->SetChannels(this->decoderChannels);
            this->recycledBuffers.push_back(buffer);
            offset += this->samplesPerBuffer;
        }
    }

    return true;
}

inline Buffer* Stream::GetEmptyBuffer() {
    if (recycledBuffers.size()) {
        Buffer* target = recycledBuffers.front();
        recycledBuffers.pop_front();
        return target;
    }
    return nullptr;
}

IBuffer* Stream::GetNextProcessedOutputBuffer() {
    /* Do not repeatedly fill the entire pool before the output has had a
    chance to play it. In particular, a slow source can block the player
    thread even though decoded audio is already waiting here. Refill in the
    existing quarter-pool batches when the ready queue falls below one eighth. */
    const auto lowWater = static_cast<size_t>(std::max(1, this->bufferCount / 8));
    if (!this->rawBuffer || this->filledBuffers.size() < lowWater) {
        this->RefillInternalBuffers();
    }

    /* in the normal case we have buffers available in the filled queue. */
    if (this->filledBuffers.size()) {
        Buffer* buffer = this->filledBuffers.front();
        this->filledBuffers.pop_front();

        for (const auto& dsp : this->dsps) {
            const auto status = dsp->Process(*buffer);
            if (status != MC_OK) {
                /* a processor can fail after changing samples. never send
                that partial block to the output. */
                this->decodeStatus = status;
                this->done = true;
                this->recycledBuffers.push_back(buffer);
                return nullptr;
            }
        }

        return buffer;
    }

    return nullptr;
}

void Stream::RefillInternalBuffers() {
    int recycled = (int) this->recycledBuffers.size();
    int count = 0;

    if (!this->rawBuffer) { /* not initialized */
        count = -1;
    }
    else {
        /* fill another chunk -- most of the time for file-based
        streams this will only be a single buffer. note the - 1
        part is to leave space for any potential remainder. */
        count = std::min(recycled - 1, std::max(1, this->bufferCount / 4));
    }

    Buffer* target = nullptr;
    long targetSampleOffset = 0;
    long targetSamplesRemain = 0;

    while (!this->done && (count > 0 || count == -1)) {
        /* get the next buffer, if the last one has been consumed... */
        if (this->decoderSamplesRemain <= 0) {
            if (!GetNextBufferFromDecoder()) {
                if (target) { /* very last buffer for this stream. */
                    target->SetSamples(targetSampleOffset);
                }
                this->done = true;
                break;
            }

            this->decoderSamplesRemain = static_cast<long>(this->decoded.frame_count * this->decoderChannels);
            this->decoderSampleOffset = 0;
        }

        /* count will be < 0 on the very first pass through. let's try to
        fill 1/4 of our buffers */
        if (count < 0) {
            count = bufferCount / 4;
        }

        /* we're going to write to this guy... */
        if (!target) {
            target = this->GetEmptyBuffer();

            if (!target) {
                break; /* no available buffers. break out. */
            }

            target->SetSamples(0);

            target->SetPosition(
                ((double) this->decoderPosition) /
                ((double) this->decoderChannels) /
                ((double) this->decoderSampleRate));

            filledBuffers.push_back(target);
        }

        /* write to the target, from the decoder buffer. note that after the
        write the target may not be full, or the decoder buffer may not be
        empty. we'll go through the loop again... */
        targetSamplesRemain = this->samplesPerBuffer - targetSampleOffset;
        if (targetSamplesRemain > 0) {
            long samplesToCopy = std::min(this->decoderSamplesRemain, targetSamplesRemain);
            if (samplesToCopy > 0) {
                const float* src = this->decoded.data + this->decoderSampleOffset;
                target->Copy(src, samplesToCopy, targetSampleOffset);

                this->decoderPosition += samplesToCopy;
                this->decoderSampleOffset += samplesToCopy;
                this->decoderSamplesRemain -= samplesToCopy;

                targetSampleOffset += samplesToCopy;

                if (targetSampleOffset == this->samplesPerBuffer) {
                    targetSampleOffset = 0;
                    target = nullptr;
                    --count; /* target buffer has been filled. */
                }
            }
        }
    }
}
