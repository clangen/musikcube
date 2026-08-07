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

#pragma once

/** @file Stream.h
 *  @brief IStream implementation that decodes a URI and applies DSP plugins.
 *  @details Buffers decoded PCM from a decoder, runs it through a chain of DSP
 *      plugins, and hands processed interleaved buffers to a Player. Buffers are
 *      recycled through an internal pool to avoid re-allocation. */

#include <musikcore/config.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/IStream.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IOutput.h>

#include <musikcore/sdk/IDSP.h>
#include <musikcore/sdk/constants.h>

#include <deque>
#include <list>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Decodes and DSP-processes a URI, producing buffers for a Player.
     *  @details Maintains a pool of recycled and filled buffers. Decoder output is
     *      accumulated into fixed-size buffers, processed through the DSP chain and
     *      handed to the player. Supports seeking, buffering, and capability
     *      queries. */
    class Stream : public IStream {
        using IDSP = musik::core::sdk::IDSP; /**< DSP plugin alias. */
        using IDecoder = musik::core::sdk::IDecoder; /**< Decoder alias. */
        using IBuffer = musik::core::sdk::IBuffer; /**< SDK buffer alias. */
        using StreamFlags = musik::core::sdk::StreamFlags; /**< Stream option flags. */

        public:
            /** @brief Creates a shared stream.
             *  @param samplesPerChannel Samples per channel per output buffer.
             *  @param bufferLengthSeconds Approximate buffered audio length.
             *  @param options Stream option flags.
             *  @return A shared IStream ready to be opened. */
            static IStreamPtr Create(
                int samplesPerChannel = 2048,
                double bufferLengthSeconds = 5,
                StreamFlags options = StreamFlags::None);

            /** @brief Creates an unmanaged (raw pointer) stream.
             *  @param samplesPerChannel Samples per channel per output buffer.
             *  @param bufferLengthSeconds Approximate buffered audio length.
             *  @param options Stream option flags.
             *  @return A raw IStream; release with Release(). */
            static IStream* CreateUnmanaged(
                int samplesPerChannel = 2048,
                double bufferLengthSeconds = 5,
                StreamFlags options = StreamFlags::None);

        private:
            /** @brief Constructs a stream (use the static factory methods).
             *  @param samplesPerChannel Samples per channel per output buffer.
             *  @param bufferLengthSeconds Approximate buffered audio length.
             *  @param options Stream option flags. */
            Stream(
                int samplesPerChannel,
                double bufferLengthSeconds,
                StreamFlags options);

        public:
            virtual ~Stream();

            /** @return The next fully processed output buffer.
             *  @details Blocks or returns quickly depending on buffer availability;
             *      returns nullptr when EOF is reached. */
            IBuffer* GetNextProcessedOutputBuffer() override;
            /** @brief Returns a processed buffer to the pool.
             *  @param buffer The buffer to recycle. */
            void OnBufferProcessedByPlayer(IBuffer* buffer) override;
            /** @brief Seeks within the stream.
             *  @param seconds The target position, in seconds.
             *  @return The achieved position, in seconds. */
            double SetPosition(double seconds) override;
            /** @return The total duration, in seconds. */
            double GetDuration() override;
            /** @brief Opens the stream for the given URI.
             *  @param uri The URI to open.
             *  @param output The output device to format for.
             *  @return true on success. */
            bool OpenStream(std::string uri, musik::core::sdk::IOutput* output) override;
            /** @brief Interrupts the stream (used on stop). */
            void Interrupt() override;
            /** @return Bitmask of supported sdk::Capability values. */
            int GetCapabilities() override;
            /** @return true when the stream has reached EOF. */
            bool Eof() override { return this->done; }
            /** @brief Frees the stream (deletes this instance). */
            void Release() override { delete this; }

        private:
            /** @brief Decodes the next chunk from the decoder into internal buffers. */
            bool GetNextBufferFromDecoder();
            /** @return A recycled (empty) buffer, or nullptr if none available. */
            Buffer* GetEmptyBuffer();
            /** @brief Refills the internal buffer pool from the decoder. */
            void RefillInternalBuffers();

            typedef std::deque<Buffer*> BufferList; /**< FIFO buffer queue alias. */
            typedef std::shared_ptr<IDecoder> DecoderPtr; /**< Decoder pointer alias. */
            typedef std::shared_ptr<IDSP> DspPtr; /**< DSP pointer alias. */
            typedef std::vector<DspPtr> Dsps; /**< DSP chain alias. */

            long decoderSampleRate; /**< Sample rate reported by the decoder. */
            long decoderChannels;   /**< Channel count reported by the decoder. */
            std::string uri;        /**< Currently open URI. */
            musik::core::io::DataStreamFactory::DataStreamPtr dataStream; /**< Underlying data stream. */

            BufferList recycledBuffers; /**< Buffers available for reuse. */
            BufferList filledBuffers;   /**< Buffers ready for the player. */

            Buffer* decoderBuffer;     /**< Scratch buffer for decoder output. */
            long decoderSampleOffset;  /**< Offset into decoderBuffer. */
            long decoderSamplesRemain; /**< Samples left in decoderBuffer. */
            uint64_t decoderPosition;  /**< Absolute decoded sample position. */

            musik::core::sdk::StreamFlags options; /**< Stream option flags. */
            int samplesPerChannel;   /**< Samples per channel per buffer. */
            long samplesPerBuffer;   /**< Total samples per buffer (frames * channels). */
            int bufferCount;         /**< Number of buffers in the pool. */
            bool done;               /**< true when EOF has been reached. */
            double bufferLengthSeconds; /**< Target buffered audio length. */
            int capabilities;        /**< Cached capability bitmask. */

            float* rawBuffer; /**< Scratch storage for decoder output. */

            DecoderPtr decoder; /**< Active decoder. */
            Dsps dsps;          /**< DSP plugin chain. */
    };

} } }
