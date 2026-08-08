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

///

/// @file TranscodingAudioDataStream.h
/// @brief On-demand streaming transcode data stream.
/// @details Decodes a source URI in real time, re-encodes it to the requested
/// format with a streaming encoder and exposes the result as an IDataStream so
/// a remote client can stream it. Used when the encoder supports on-demand
/// (streaming) transcoding.

#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IStreamingEncoder.h>
#include <musikcore/sdk/DataBuffer.h>
#include "Context.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <stdio.h>

/** @brief IDataStream that decodes and re-encodes audio on demand.
 *  @details Reads PCM from the decoded source, feeds it through an
 *  IStreamingEncoder and buffers the encoded output. The stream can optionally
 *  write the encoded data to a temporary file for later promotion into the
 *  transcode cache. */
class TranscodingAudioDataStream : public musik::core::sdk::IDataStream {
    public:
        /** @brief Position type alias. */
        using PositionType = musik::core::sdk::PositionType;
        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        /** @brief Constructs a transcode stream without caching.
         *  @param context Shared server context.
         *  @param encoder The streaming encoder to use.
         *  @param uri URI of the source track.
         *  @param bitrate Target bitrate.
         *  @param format Target format name. */
        TranscodingAudioDataStream(
            Context& context,
            musik::core::sdk::IStreamingEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        /** @brief Constructs a transcode stream with cache files.
         *  @param context Shared server context.
         *  @param encoder The streaming encoder to use.
         *  @param uri URI of the source track.
         *  @param tempFilename Temporary output file.
         *  @param finalFilename Final cached output file.
         *  @param bitrate Target bitrate.
         *  @param format Target format name. */
        TranscodingAudioDataStream(
            Context& context,
            musik::core::sdk::IStreamingEncoder* encoder,
            const std::string& uri,
            const std::string& tempFilename,
            const std::string& finalFilename,
            size_t bitrate,
            const std::string& format);

        /** @brief Destroys the stream and releases resources. */
        virtual ~TranscodingAudioDataStream();

        /** @brief Opens the stream, decoding and encoding the source.
         *  @param uri The source URI.
         *  @param flags Open flags.
         *  @return True if the stream opened successfully. */
        virtual bool Open(const char *uri, OpenFlags flags) override;
        /** @brief Closes the stream and finalizes the cache file.
         *  @return True on success. */
        virtual bool Close() override;
        /** @brief Interrupts any blocking operation. */
        virtual void Interrupt() override;
        /** @brief Destroys the stream. */
        virtual void Release() override;
        /** @brief Returns whether the stream can be read.
         *  @return Always returns true. */
        virtual bool Readable() override { return true; }
        /** @brief Returns whether the stream can be written.
         *  @return Always returns false. */
        virtual bool Writable() override { return false; };
        /** @brief Reads the next block of encoded audio.
         *  @param buffer Destination buffer.
         *  @param readBytes Number of bytes to read.
         *  @return Number of bytes read, or 0 at end of stream. */
        virtual PositionType Read(void *buffer, PositionType readBytes) override;
        /** @brief Writing is not supported.
         *  @return Always returns 0. */
        virtual PositionType Write(void *buffer, PositionType writeBytes) override { return 0; }
        /** @brief Seeks within the encoded output (limited support).
         *  @param position Target byte offset.
         *  @return True if the seek succeeded. */
        virtual bool SetPosition(PositionType position) override;
        /** @brief Returns the current byte offset.
         *  @return Current position in bytes. */
        virtual PositionType Position() override;
        /** @brief Returns whether the stream supports seeking.
         *  @return True if seeking is possible. */
        virtual bool Seekable() override;
        /** @brief Returns whether end of stream has been reached.
         *  @return True at end of the encoded data. */
        virtual bool Eof() override;
        /** @brief Returns the encoded length in bytes.
         *  @return Length in bytes, or -1 if unknown. */
        virtual long Length() override;
        /** @brief Returns the stream type.
         *  @return The stream type string. */
        virtual const char* Type() override;
        /** @brief Returns the stream URI.
         *  @return The URI this stream was opened with. */
        virtual const char* Uri() override;
        /** @brief Returns whether the stream can be prefetched.
         *  @return True if prefetching is supported. */
        virtual bool CanPrefetch() override;

        /** @brief Returns the number of active transcode streams.
         *  @return Active stream count. */
        static int GetActiveCount();

    private:
        /** @brief Releases the decoder, encoder and file handles. */
        void Dispose();

        /** @brief Shared server context. */
        Context& context;
        /** @brief Input stream opened on the source URI. */
        musik::core::sdk::IDataStream* input;
        /** @brief Decoder producing PCM from the source. */
        musik::core::sdk::IDecoder* decoder;
        /** @brief Scratch PCM buffer used between decode and encode. */
        musik::core::sdk::IBuffer* pcmBuffer;
        /** @brief The streaming encoder being driven. */
        musik::core::sdk::IStreamingEncoder* encoder;
        /** @brief Buffer holding encoded data not yet consumed. */
        DataBuffer<char> spillover;
        /** @brief Target bitrate. */
        size_t bitrate;
        /** @brief Whether end of encoded data has been reached. */
        bool eof;
        /** @brief Guards reads and state changes. */
        std::mutex mutex;
        /** @brief Current and total encoded length. */
        PositionType length, position;
        /** @brief Handle of the optional cache file. */
        FILE* outFile;
        /** @brief Temporary and final cache file names. */
        std::string tempFilename, finalFilename;
        /** @brief Target format name. */
        std::string format;
        /** @brief Whether the stream was interrupted or the encoder started. */
        bool interrupted{ false }, encoderInitialized{ false };
        /** @brief Byte tolerance before a detached client stream is dropped. */
        long detachTolerance;
};