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

/** @file LocalFileStream.h
 *  @brief IDataStream implementation for local filesystem files.
 *  @details Uses stdio FILE* for buffered file access. Supports reading, writing,
 *      seeking and EOF detection, and exposes file type/URI metadata. Open/Close
 *      flag state determines whether the stream is readable or writable. */

#include <musikcore/config.h>
#include <musikcore/sdk/IDataStream.h>
#include <atomic>

/** @namespace musik::core::io
 *  @brief Input/output helpers: data streams and stream factories. */
namespace musik { namespace core { namespace io {

    /** @brief Provides read/write access to a local file.
     *  @details The underlying FILE* is held atomically so Interrupt() can
     *      asynchronously close it from another thread. Files opened with the Read
     *      flag are readable; the Write flag enables writing. */
    class LocalFileStream : public musik::core::sdk::IDataStream {
        public:
            using PositionType = musik::core::sdk::PositionType; /**< Byte offset alias. */
            using OpenFlags = musik::core::sdk::OpenFlags; /**< Open flag alias. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(LocalFileStream)

            /** @brief Creates an unopened local file stream. */
            LocalFileStream() noexcept;
            virtual ~LocalFileStream() noexcept;

            /** @brief Opens the given file.
             *  @param filename The file path to open.
             *  @param flags Read/Write access flags.
             *  @return true on success. */
            bool Open(const char *filename, OpenFlags flags) override;
            /** @brief Closes the file.
             *  @return true on success. */
            bool Close() noexcept override;
            /** @brief Asynchronously closes the file to interrupt a blocking read. */
            void Interrupt() noexcept override;
            /** @brief Closes the stream and frees it (deletes this instance). */
            void Release() noexcept override;
            /** @return true if the stream was opened with the Read flag. */
            bool Readable() noexcept override { return (flags & OpenFlags::Read) != 0; }
            /** @return true if the stream was opened with the Write flag. */
            bool Writable() noexcept override { return (flags & OpenFlags::Write) != 0; }
            /** @brief Reads bytes from the file.
             *  @param buffer Destination buffer.
             *  @param readBytes Number of bytes to read.
             *  @return The number of bytes actually read. */
            PositionType Read(void* buffer, PositionType readBytes) noexcept override;
            /** @brief Writes bytes to the file.
             *  @param buffer Source buffer.
             *  @param writeBytes Number of bytes to write.
             *  @return The number of bytes actually written. */
            PositionType Write(void* buffer, PositionType writeBytes) noexcept override;
            /** @brief Seeks to a byte offset.
             *  @param position The target offset.
             *  @return true on success. */
            bool SetPosition(PositionType position) noexcept override;
            /** @return The current byte offset. */
            PositionType Position() noexcept override;
            /** @return true when the file position is at end-of-file. */
            bool Eof() noexcept override;
            /** @return The total length of the file, in bytes. */
            long Length() noexcept override;
            /** @return true (local files are always seekable). */
            bool Seekable() noexcept override;
            /** @return The stream type string ("LocalFileStream"). */
            const char* Type() noexcept override;
            /** @return The URI of the opened file. */
            const char* Uri() noexcept override;
            /** @return true (local files can be prefetched). */
            bool CanPrefetch() noexcept override { return true; }

        private:
            OpenFlags flags { OpenFlags::None }; /**< Open flags (read/write). */
            std::string extension; /**< File extension of the opened path. */
            std::string uri;       /**< Path of the opened file. */
            std::atomic<FILE*> file; /**< Underlying FILE*, atomically held. */
            long filesize;         /**< Cached file length, in bytes. */
    };

} } }