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

/// @file GmeDataStream.h
/// @brief IDataStream adapter used by the Game Music Emu (GME) decoder.
/// @details Wraps an existing IDataStream and parses chiptune URIs of the form
/// "gme://file#<track>" or a plain file path. It forwards all IDataStream
/// operations to the wrapped stream while exposing the selected sub-track number
/// and the underlying filename to the decoder.

#include <musikcore/sdk/IDataStream.h>
#include <string>

/** @brief IDataStream adapter for GME chiptune files.
 *  @details A GME archive can contain several sub-tracks. The URI fragment
 *  selects which track to play; this class keeps that number and the resolved
 *  filename available to the GmeDecoder while delegating byte-level I/O to the
 *  wrapped stream. */
class GmeDataStream: public musik::core::sdk::IDataStream {
    public:
        /** @brief Position type alias. */
        using PositionType = musik::core::sdk::PositionType;
        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        /** @brief Constructs an empty wrapper with no underlying stream. */
        GmeDataStream();
        /** @brief Wraps an existing stream.
         *  @param stream The stream to wrap and delegate to. */
        GmeDataStream(musik::core::sdk::IDataStream* stream);
        /** @brief Destroys the wrapper. */
        virtual ~GmeDataStream();

        /** @brief Opens a chiptune URI, parsing the track number if present.
         *  @param uri The gme:// URI or file path to open.
         *  @param flags Open flags.
         *  @return True if the URI was parsed and the stream opened. */
        virtual bool Open(const char *uri, OpenFlags flags) override;
        /** @brief Closes the wrapped stream.
         *  @return True on success. */
        virtual bool Close() override;
        /** @brief Interrupts any blocking operation on the wrapped stream. */
        virtual void Interrupt() override;
        /** @brief Destroys the wrapper, releasing the wrapped stream. */
        virtual void Release() override;
        /** @brief Returns whether the stream can be read.
         *  @return Always returns true. */
        virtual bool Readable() override { return true; }
        /** @brief Returns whether the stream can be written.
         *  @return Always returns false. */
        virtual bool Writable() override { return false; }
        /** @brief Reads bytes from the wrapped stream.
         *  @param buffer Destination buffer.
         *  @param readBytes Number of bytes to read.
         *  @return Number of bytes read, or 0 at end of stream. */
        virtual PositionType Read(void *buffer, PositionType readBytes) override;
        /** @brief Writing is not supported.
         *  @return Always returns 0. */
        virtual PositionType Write(void *buffer, PositionType writeBytes) override { return 0; }
        /** @brief Seeks to a byte offset in the wrapped stream.
         *  @param position Target byte offset.
         *  @return True if the seek succeeded. */
        virtual bool SetPosition(PositionType position) override;
        /** @brief Returns the current byte offset.
         *  @return Current position in bytes. */
        virtual PositionType Position() override;
        /** @brief Returns whether the stream supports seeking.
         *  @return True if the wrapped stream is seekable. */
        virtual bool Seekable() override;
        /** @brief Returns whether end of stream has been reached.
         *  @return True at end of stream. */
        virtual bool Eof() override;
        /** @brief Returns the stream length in bytes.
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

        /** @brief Parses a chiptune URI into filename and track number.
         *  @param uri The URI to parse.
         *  @return True if the URI was understood. */
        bool Parse(const char* uri);
        /** @brief Returns the selected sub-track number.
         *  @return Zero-based track number. */
        int GetTrackNumber() { return this->trackNumber; }
        /** @brief Returns the resolved chiptune filename.
         *  @return The underlying file path. */
        std::string GetFilename() { return this->filename; }

    private:
        /** @brief Zero-based sub-track number selected by the URI. */
        int trackNumber { 0 };
        /** @brief Resolved path of the chiptune file. */
        std::string filename;
        /** @brief The wrapped underlying data stream. */
        musik::core::sdk::IDataStream* stream { nullptr };
        /** @brief Whether this wrapper owns and releases the stream. */
        bool releaseStream{ true };
};