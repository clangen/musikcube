//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "stdafx.h"
#include "CddaDataStream.h"
#include <string>
#include <set>
#include <boost/thread/mutex.hpp>

#define RAW_SECTOR_SIZE 2352
#define MSF2UINT(hgs) ((hgs[1]*4500) + (hgs[2]*75) + (hgs[3]))

static CddaDataStream* active = NULL;
static boost::mutex activeMutex;

static void setActive(CddaDataStream* stream) {
    boost::mutex::scoped_lock lock(activeMutex);

    if (active != NULL) {
        active->Close();
        active = NULL;
    }

    active = stream;
}

static void resetIfActive(CddaDataStream* stream) {
    boost::mutex::scoped_lock lock(activeMutex);

    if (stream == active) {
        active = NULL;
    }
}

CddaDataStream::CddaDataStream() {
    this->closed = false;
    this->drive = INVALID_HANDLE_VALUE;
    this->position = this->length = 0;
    memset(&this->toc, 0, sizeof(this->toc));
    this->startSector = this->stopSector = 0;
}

CddaDataStream::~CddaDataStream() {
    this->Close();
    resetIfActive(this);
}

int CddaDataStream::GetChannelCount() {
    return this->channels;
}

static int getTrackNumber(const char* uri) {
    std::string filename(uri);
    size_t lastDot = filename.find_last_of(".");
    if (lastDot != std::string::npos) {
        /* always in the format F:\Track01.cda */
        std::string number = filename.substr(lastDot - 2, 2);
        try {
            return stoi(number);
        }
        catch (...) {
            return 1;
        }
    }

    return 1;
}

bool CddaDataStream::Open(const char *uri, unsigned int options) {
    int trackIndex;

    char driveLetter = 'A' + PathGetDriveNumber(uri);
    std::string drivePath = "\\\\.\\" + std::string(1, driveLetter) + ":";

    this->drive = CreateFile(
        drivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
        (HANDLE) NULL);

    if (this->drive == INVALID_HANDLE_VALUE) {
        return false;
    }

    trackIndex = getTrackNumber(uri);

    DWORD byteCount;

    BOOL canReadFromDevice = DeviceIoControl(
        this->drive,
        IOCTL_CDROM_READ_TOC,
        NULL,
        0,
        &this->toc,
        sizeof(this->toc),
        &byteCount,
        0);

    bool trackIndexValid =
        this->toc.FirstTrack <= trackIndex &&
        trackIndex <= this->toc.LastTrack;

    if (!canReadFromDevice && !trackIndexValid) {
        this->Close();
        return false;
    }

    /* MMC-3 Draft Revision 10g: Table 222 – Q Sub-channel control field */
    this->toc.TrackData[trackIndex - 1].Control &= 5;
    if (!(
        this->toc.TrackData[trackIndex - 1].Control == 0 ||
        this->toc.TrackData[trackIndex - 1].Control == 1))
    {
        this->Close();
        return(false);
    }

    this->channels = 2;
    if (this->toc.TrackData[trackIndex - 1].Control & 8) {
        this->channels = 4;
    }

    this->startSector = MSF2UINT(this->toc.TrackData[trackIndex - 1].Address) - 150;
    this->stopSector = MSF2UINT(this->toc.TrackData[trackIndex].Address) - 150;
    this->length = (this->stopSector - this->startSector) * RAW_SECTOR_SIZE;

    setActive(this);

    return true;
}

bool CddaDataStream::Close() {
    if (this->drive != INVALID_HANDLE_VALUE) {
        CloseHandle(this->drive);
        this->drive = INVALID_HANDLE_VALUE;
    }

    this->closed = true;

    return true;
}

void CddaDataStream::Destroy() {
    delete this;
}

PositionType CddaDataStream::Read(void* buffer, PositionType readBytes) {
    DWORD actualBytesRead = 0;
    this->Read((BYTE*) buffer, readBytes, true, &actualBytesRead);
    return (PositionType) actualBytesRead;
}

bool CddaDataStream::SetPosition(PositionType position) {
    if (position > this->length - 1) {
        return false;
    }

    this->position = position;

    while (this->position % (2 * this->channels)) {
        this->position++;
    }

    return true;
}

PositionType CddaDataStream::Position() {
    return (PositionType) this->position;
}

bool CddaDataStream::Eof() {
    return (this->position >= this->length - 1);
}

long CddaDataStream::Length() {
    return (long) this->length;
}

const char* CddaDataStream::Type() {
    return "cda";
}

HRESULT CddaDataStream::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead) {
    if (this->closed) {
        pdwBytesRead = 0;
        return S_FALSE;
    }

    BYTE buff[RAW_SECTOR_SIZE];

    PBYTE pbBufferOrg = pbBuffer;
    LONGLONG pos = this->position;
    size_t len = (size_t) dwBytesToRead;

    while (pos >= 0 && pos < this->length && len > 0) {
        RAW_READ_INFO rawreadinfo;
        rawreadinfo.SectorCount = 1;
        rawreadinfo.TrackMode = CDDA;

        UINT sector = this->startSector + int(pos / RAW_SECTOR_SIZE);
        __int64 offset = pos % RAW_SECTOR_SIZE;

        rawreadinfo.DiskOffset.QuadPart = sector * 2048;
        DWORD bytesRead = 0;

        DeviceIoControl(
            this->drive,
            IOCTL_CDROM_RAW_READ,
            &rawreadinfo,
            sizeof(rawreadinfo),
            buff, RAW_SECTOR_SIZE,
            &bytesRead,
            0);

        size_t l = (size_t)min(min(len, RAW_SECTOR_SIZE - offset), this->length - pos);
        memcpy(pbBuffer, &buff[offset], l);

        pbBuffer += l;
        pos += l;
        len -= l;
    }

    if (pdwBytesRead) {
        *pdwBytesRead = pbBuffer - pbBufferOrg;
    }

    this->position += pbBuffer - pbBufferOrg;

    return S_OK;
}
