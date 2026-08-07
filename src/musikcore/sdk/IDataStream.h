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

/** @file IDataStream.h @brief Defines the IDataStream interface for reading and writing byte streams. */
#pragma once

#include "constants.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A 64-bit position type used to address locations within a stream. */
    typedef long PositionType;

    /** @brief An abstract, seekable byte stream that can be read from and/or
     *  written to, such as a file, network source, or in-memory buffer. */
    class IDataStream {
        public:
            /** @brief Virtual destructor to allow safe polymorphic deletion. */
            virtual ~IDataStream() {}

            /** @brief Opens the stream for the requested access.
             *  @param uri The URI that identifies the stream source or destination.
             *  @param flags The access flags (e.g. OpenFlags::Read, OpenFlags::Write).
             *  @return True if the stream was opened successfully. */
            virtual bool Open(const char *uri, OpenFlags flags) = 0;

            /** @brief Closes the stream, releasing any underlying resources. */
            virtual bool Close() = 0;

            /** @brief Interrupts any in-progress blocking read or write operation. */
            virtual void Interrupt() = 0;

            /** @brief Releases the stream; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns whether the stream supports reading.
             *  @return True if the stream can be read from. */
            virtual bool Readable() = 0;

            /** @brief Returns whether the stream supports writing.
             *  @return True if the stream can be written to. */
            virtual bool Writable() = 0;

            /** @brief Reads a chunk of bytes from the stream.
             *  @param buffer The destination buffer to read into.
             *  @param readBytes The number of bytes to attempt to read.
             *  @return The number of bytes actually read, or a negative value on error. */
            virtual PositionType Read(void *buffer, PositionType readBytes) = 0;

            /** @brief Writes a chunk of bytes to the stream.
             *  @param buffer The source buffer to write from.
             *  @param writeBytes The number of bytes to attempt to write.
             *  @return The number of bytes actually written, or a negative value on error. */
            virtual PositionType Write(void *buffer, PositionType writeBytes) = 0;

            /** @brief Seeks to an absolute position in the stream.
             *  @param position The target position, in bytes from the start of the stream.
             *  @return True if the seek succeeded. */
            virtual bool SetPosition(PositionType position) = 0;

            /** @brief Returns the current position in the stream.
             *  @return The current position, in bytes from the start of the stream. */
            virtual PositionType Position() = 0;

            /** @brief Returns whether the stream supports random access seeking.
             *  @return True if the stream is seekable. */
            virtual bool Seekable() = 0;

            /** @brief Returns whether the end of the stream has been reached.
             *  @return True when all available data has been consumed. */
            virtual bool Eof() = 0;

            /** @brief Returns the total length of the stream.
             *  @return The stream length in bytes, or a negative value if unknown. */
            virtual long Length() = 0;

            /** @brief Returns the stream type identifier.
             *  @return A string describing the stream implementation type. */
            virtual const char* Type() = 0;

            /** @brief Returns the URI the stream is associated with.
             *  @return The stream URI. */
            virtual const char* Uri() = 0;

            /** @brief Returns whether the stream supports prefetching.
             *  @return True if the stream can be prefetched. */
            virtual bool CanPrefetch() = 0;
    };

} } }