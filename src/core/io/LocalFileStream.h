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

#pragma once

#include <core/config.h>
#include <core/sdk/IDataStream.h>
#include <atomic>

namespace musik { namespace core { namespace io {

    class LocalFileStream : public musik::core::sdk::IDataStream {
        public:
            using PositionType = musik::core::sdk::PositionType;
            using OpenFlags = musik::core::sdk::OpenFlags;

            LocalFileStream();
            virtual ~LocalFileStream();

            virtual bool Open(const char *filename, OpenFlags flags);
            virtual bool Close();
            virtual void Interrupt();
            virtual void Release();
            virtual bool Readable() { return (flags & OpenFlags::Read) != 0; }
            virtual bool Writable() { return (flags & OpenFlags::Write) != 0; }
            virtual PositionType Read(void* buffer, PositionType readBytes);
            virtual PositionType Write(void* buffer, PositionType writeBytes);
            virtual bool SetPosition(PositionType position);
            virtual PositionType Position();
            virtual bool Eof();
            virtual long Length();
            virtual bool Seekable();
            virtual const char* Type();
            virtual const char* Uri();
            virtual bool CanPrefetch() { return true; }

        private:
            OpenFlags flags { OpenFlags::None };
            std::string extension;
            std::string uri;
            std::atomic<FILE*> file;
            long filesize;
    };

} } }