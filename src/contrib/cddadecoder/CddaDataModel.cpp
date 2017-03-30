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
#include <string>
#include "CddaDataModel.h"

using AudioDiscPtr = CddaDataModel::AudioDiscPtr;
using DiscTrackPtr = CddaDataModel::DiscTrackPtr;

CddaDataModel::CddaDataModel() {

}

CddaDataModel::~CddaDataModel() {

}

std::vector<AudioDiscPtr> CddaDataModel::GetAudioDiscs() {
    std::vector<AudioDiscPtr> result;

    HANDLE driveHandle = nullptr;
    DWORD byteCount;

    for (int i = 0; i < 26; i++) {
        char driveLetter = (char)('A' + i);
        std::string drive = std::string(1, driveLetter) + ":";

        if (GetDriveTypeA(drive.c_str()) == DRIVE_CDROM) {
            drive = "\\\\.\\" + std::string(1, driveLetter) + ":";

            driveHandle = CreateFileA(
                drive.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
                (HANDLE) nullptr);

            if (driveHandle == INVALID_HANDLE_VALUE) {
                continue;
            }

            CDROM_TOC toc = { 0 };

            BOOL readable = DeviceIoControl(
                driveHandle,
                IOCTL_CDROM_READ_TOC,
                NULL,
                0,
                &toc,
                sizeof(toc),
                &byteCount,
                0);

            if (readable) {
                auto audioDisc = std::shared_ptr<AudioDisc>(new AudioDisc(driveLetter));

                int trackCount = (toc.LastTrack - toc.FirstTrack) + 1;

                for (int j = 0; j < trackCount; j++) {
                    if (toc.TrackData[j].Control & 4) {
                        continue; /* data track */
                    }

                    auto start = MSF2UINT(toc.TrackData[j].Address) - FRAMES_PER_PREGAP;
                    auto end = MSF2UINT(toc.TrackData[j + 1].Address) - FRAMES_PER_PREGAP;
                    auto length = (end - start) * BYTES_PER_SECTOR;
                    double duration = (double)(length / 2 / sizeof(short)) / 44100.0f;

                    audioDisc->AddTrack(std::make_shared<DiscTrack>(
                        toc.TrackData[j], driveLetter, j, duration));
                }

                if (audioDisc->GetTrackCount()) {
                    audioDisc->SetLeadout(std::make_shared<DiscTrack>(
                        toc.TrackData[trackCount], driveLetter, trackCount + 1, 0));

                    result.push_back(audioDisc);
                }
            }
        }
    }

    return result;
}

/* * * * CddaDataModel::DiscTrack * * * */

CddaDataModel::DiscTrack::DiscTrack(TRACK_DATA& data, const char driveLetter, int number, double duration) {
    this->driveLetter = driveLetter;
    this->number = number;
    this->duration = duration;
    this->minutes = data.Address[1];
    this->seconds = data.Address[2];
    this->frames = data.Address[3];
}

/* http://www.robots.ox.ac.uk/~spline/cddb-howto.txt, appendix a */
int CddaDataModel::DiscTrack::GetCddbSum() {
    int n = (this->minutes * 60) + this->seconds;
    int ret = 0;

    while (n > 0) {
        ret += (n % 10);
        n /= 10;
    }

    return ret;
}

/* * * * CddaDataModel::AudioDisc * * * */

CddaDataModel::AudioDisc::AudioDisc(char driveLetter) {
    this->driveLetter = driveLetter;
}

std::string CddaDataModel::AudioDisc::GetCddbId() {
    int n = 0;

    for (int i = 0; i < this->GetTrackCount(); i++) {
        n += this->GetTrackAt(i)->GetCddbSum();
    }

    auto first = this->GetTrackAt(0);
    auto last = this->leadout;

    int t = ((last->GetMinutes() * 60) + last->GetSeconds()) -
        ((first->GetMinutes() * 60) + first->GetSeconds());

    int ret = ((n % 0xff) << 24 | t << 8 | this->GetTrackCount());

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%08x", ret);
    return std::string(buffer);
}

void CddaDataModel::AudioDisc::SetLeadout(DiscTrackPtr leadout) {
    this->leadout = leadout;
}

void CddaDataModel::AudioDisc::AddTrack(DiscTrackPtr track) {
    this->tracks.push_back(track);
}

int CddaDataModel::AudioDisc::GetTrackCount() {
    return (int) this->tracks.size();
}

DiscTrackPtr CddaDataModel::AudioDisc::GetTrackAt(int index) {
    return this->tracks.at(index);
}

std::string CddaDataModel::DiscTrack::GetFilePath() {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%c:\\%02d.cda", this->driveLetter, this->number + 1);
    return std::string(buffer);
}