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

#include "stdafx.h"
#include "CddaDataStream.h"
#include <algorithm>
#include <string>
#include <set>
#include <mutex>

static std::mutex driveAccessMutex; /* one track can read at a time */
static CddaDataStream* activeRead = nullptr;

CddaDataStream::CddaDataStream() {
    this->closed = false;
    this->drive = INVALID_HANDLE_VALUE;
    this->position = this->length = 0;
    memset(&this->toc, 0, sizeof(this->toc));
    this->startSector = this->stopSector = 0;
}

CddaDataStream::~CddaDataStream() {
    this->Close();
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

bool CddaDataStream::Open(const char *uri, OpenFlags flags) {
    int trackIndex;

    char driveLetter = 'A' + PathGetDriveNumberA(uri);
    std::string drivePath = "\\\\.\\" + std::string(1, driveLetter) + ":";

    this->drive = CreateFileA(
        drivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
        (HANDLE) nullptr);

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

    if (!canReadFromDevice || !trackIndexValid) {
        this->Close();
        return false;
    }

    /* MMC-3 Draft Revision 10g: Table 222 � Q Sub-channel control field */
    if (this->toc.TrackData[trackIndex - 1].Control & 4) {
        /* nope, got a data track. */
        this->Close();
        return(false);
    }

    this->channels = 2;
    this->startSector = MSF2UINT(this->toc.TrackData[trackIndex - 1].Address) - FRAMES_PER_PREGAP;
    this->stopSector = MSF2UINT(this->toc.TrackData[trackIndex].Address) - FRAMES_PER_PREGAP;
    this->length = (this->stopSector - this->startSector) * BYTES_PER_SECTOR;
    this->uri = uri;

    return true;
}

bool CddaDataStream::Close() {
    if (this->drive != INVALID_HANDLE_VALUE) {
        CloseHandle(this->drive);
        this->drive = INVALID_HANDLE_VALUE;
    }

    {
        std::lock_guard<std::mutex> lock(driveAccessMutex);
        if (activeRead == this) {
            activeRead = nullptr;
        }
    }

    this->closed = true;
    return true;
}

void CddaDataStream::Interrupt() {

}

void CddaDataStream::Release() {
    delete this;
}

PositionType CddaDataStream::Read(void* buffer, PositionType readBytes) {
    {
        std::lock_guard<std::mutex> lock(driveAccessMutex);
        if (activeRead == nullptr) {
            activeRead = this;
        }
    }

    if (activeRead != this) {
        return (PositionType) ReadError::DeviceBusy;
    }

    if (this->position >= this->length) {
        return 0;
    }

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

bool CddaDataStream::Seekable() {
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

const char* CddaDataStream::Uri() {
    return uri.c_str();
}

HRESULT CddaDataStream::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead) {
    if (this->closed) {
        pdwBytesRead = 0;
        return S_FALSE;
    }

    DWORD readSize = 0;
    LONGLONG pos = this->position;
    UINT sectorOffset = this->startSector + (int)(pos / BYTES_PER_SECTOR);

    RAW_READ_INFO rawReadInfo = { 0 };
    rawReadInfo.SectorCount = dwBytesToRead / BYTES_PER_SECTOR;
    rawReadInfo.TrackMode = CDDA;
    rawReadInfo.DiskOffset.QuadPart = sectorOffset * 2048;

    DeviceIoControl(
        this->drive,
        IOCTL_CDROM_RAW_READ,
        &rawReadInfo,
        sizeof(rawReadInfo),
        pbBuffer,
        dwBytesToRead,
        &readSize,
        0);

    this->position += readSize;

    if (pdwBytesRead) {
        *pdwBytesRead = readSize;
    }

    return S_OK;
}
