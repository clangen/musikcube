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
/// @file CddaDataStream.h
/// @brief IDataStream implementation that reads raw audio sectors from a CD-ROM.
/// @details Uses the Windows IOCTL_CDROM_RAW_READ control code to read 2352-byte
/// raw sectors (including subchannel data) from a CD drive. Each track is
/// exposed as a separate stream addressed by its track number in the URI.
/// Windows-only.
///

#include "config.h"
#include <musikcore/sdk/IDataStream.h>
#include <string>
#include <mutex>

using namespace musik::core::sdk;

/** @brief Reads raw PCM sectors from an audio CD.
 *  @details The stream URI selects a track on a specific drive, e.g.
 *  "cdda://D:/track/3". Reads perform low-level sector I/O against the CD-ROM
 *  device so the decoder receives the exact 16-bit stereo PCM stored on disc.
 */
class CddaDataStream : public IDataStream {
    public:
        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        /** @brief Stream read error codes. */
        enum class ReadError : int {
            /** @brief The drive reported the device was busy. */
            DeviceBusy = -128
        };

        CddaDataStream();
        ~CddaDataStream();

        /** @brief Destroys the stream and closes the drive handle. */
        void Release() override;
        /** @brief Opens the track identified by the URI.
         *  @param filename The cdda:// URI of the track to open.
         *  @param flags Open flags (ignored).
         *  @return True if the track opened successfully. */
        bool Open(const char* filename, OpenFlags flags) override;
        /** @brief Closes the stream and the drive handle.
         *  @return True on success. */
        bool Close() override;
        /** @brief Interrupts any blocking read on the drive. */
        void Interrupt();
        /** @brief Returns whether the stream can be read.
         *  @return Always returns true. */
        bool Readable() noexcept override { return true; }
        /** @brief Returns whether the stream can be written.
         *  @return Always returns false. */
        bool Writable() noexcept override { return false; }
        /** @brief Reads bytes from the track.
         *  @param buffer Destination buffer.
         *  @param readBytes Number of bytes to read.
         *  @return Number of bytes read, or a negative ReadError on failure. */
        PositionType Read(void* buffer, PositionType readBytes) override;
        /** @brief Writing is not supported.
         *  @return Always returns false. */
        PositionType Write(void* buffer, PositionType writeBytes) noexcept override { return false; }
        /** @brief Seeks to a byte offset within the track.
         *  @param position The target byte offset.
         *  @return True if the seek was successful. */
        bool SetPosition(PositionType position) override;
        /** @brief Returns the current byte offset.
         *  @return Current position in bytes. */
        PositionType Position() override;
        /** @brief Returns whether the end of the track has been reached.
         *  @return True at end of track. */
        bool Eof() override;
        /** @brief Returns the track length in bytes.
         *  @return Length in bytes, or -1 if unknown. */
        long Length() override;
        /** @brief Returns whether the stream supports seeking.
         *  @return True. */
        bool Seekable() override;
        /** @brief Returns the stream type.
         *  @return The stream type string. */
        const char* Type() override;
        /** @brief Returns the stream URI.
         *  @return The URI the stream was opened with. */
        const char* Uri() override;
        /** @brief Prefetching is not supported.
         *  @return Always returns false. */
        bool CanPrefetch() noexcept override { return false; }

        /** @brief Returns the number of audio channels in the track.
         *  @return Channel count (typically 2). */
        int GetChannelCount();

    private:
        /** @brief Performs a raw sector read from the drive.
         *  @param pbBuffer Destination buffer.
         *  @param dwBytesToRead Number of bytes to read.
         *  @param bAlign Whether the read must be sector-aligned.
         *  @param pdwBytesRead Receives the number of bytes actually read.
         *  @return S_OK on success, or an HRESULT error code. */
        HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);

        /** @brief URI this stream was opened with. */
        std::string uri;
        /** @brief Current and total byte positions within the track. */
        LONGLONG position, length;
        /** @brief Handle to the opened CD-ROM device. */
        HANDLE drive;
        /** @brief Table of contents of the disc. */
        CDROM_TOC toc;
        /** @brief Sector addresses describing the track extent. */
        UINT firstSector, startSector, stopSector;
        /** @brief Number of audio channels. */
        unsigned long channels;
        /** @brief True once the stream has been closed. */
        volatile bool closed;
};