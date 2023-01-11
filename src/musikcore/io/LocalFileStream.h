//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <musikcore/config.h>
#include <musikcore/sdk/IDataStream.h>
#include <atomic>

namespace musik { namespace core { namespace io {

    class LocalFileStream : public musik::core::sdk::IDataStream {
        public:
            using PositionType = musik::core::sdk::PositionType;
            using OpenFlags = musik::core::sdk::OpenFlags;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(LocalFileStream)

            EXPORT LocalFileStream() noexcept;
            EXPORT virtual ~LocalFileStream() noexcept;

            EXPORT bool Open(const char *filename, OpenFlags flags) override;
            EXPORT bool Close() noexcept override;
            EXPORT void Interrupt() noexcept override;
            EXPORT void Release() noexcept override;
            EXPORT bool Readable() noexcept override { return (flags & OpenFlags::Read) != 0; }
            EXPORT bool Writable() noexcept override { return (flags & OpenFlags::Write) != 0; }
            EXPORT PositionType Read(void* buffer, PositionType readBytes) noexcept override;
            EXPORT PositionType Write(void* buffer, PositionType writeBytes) noexcept override;
            EXPORT bool SetPosition(PositionType position) noexcept override;
            EXPORT PositionType Position() noexcept override;
            EXPORT bool Eof() noexcept override;
            EXPORT long Length() noexcept override;
            EXPORT bool Seekable() noexcept override;
            EXPORT const char* Type() noexcept override;
            EXPORT const char* Uri() noexcept override;
            EXPORT bool CanPrefetch() noexcept override { return true; }

        private:
            OpenFlags flags { OpenFlags::None };
            std::string extension;
            std::string uri;
            std::atomic<FILE*> file;
            long filesize;
    };

} } }