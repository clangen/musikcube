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

#include <core/sdk/IDataStream.h>

#include "ntddcdrm.h"
#include "devioctl.h"
#include <string>
#include <mutex>

using namespace musik::core::sdk;

class CddaDataStream : public IDataStream {
    public:
        using OpenFlags = musik::core::sdk::OpenFlags;

        enum class ReadError : int {
            DeviceBusy = -128
        };

        CddaDataStream();
        ~CddaDataStream();

        virtual void Release();
        virtual bool Open(const char *filename, OpenFlags flags);
        virtual bool Close();
        virtual void Interrupt();
        virtual bool Readable() { return true; }
        virtual bool Writable() { return false; }
        virtual PositionType Read(void* buffer, PositionType readBytes);
        virtual PositionType Write(void* buffer, PositionType writeBytes) { return false;  }
        virtual bool SetPosition(PositionType position);
        virtual PositionType Position();
        virtual bool Eof();
        virtual long Length();
        virtual bool Seekable();
        virtual const char* Type();
        virtual const char* Uri();
        virtual bool CanPrefetch() { return false; }

        int GetChannelCount();

    private:
        HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);

        std::string uri;
        LONGLONG position, length;
        HANDLE drive;
        CDROM_TOC toc;
        UINT firstSector, startSector, stopSector;
        unsigned long channels;
        volatile bool closed;
};