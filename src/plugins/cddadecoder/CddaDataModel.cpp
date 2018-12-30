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
#include "CddaDataModel.h"
#include <string>
#include <Windows.h>
#include <dbt.h>
#include <map>

#define CLASS_NAME L"CddaDataModelEventClass"
#define WINDOW_NAME L"CddaDataModelWindow"

using AudioDiscPtr = CddaDataModel::AudioDiscPtr;
using DiscTrackPtr = CddaDataModel::DiscTrackPtr;

static std::map<HWND, CddaDataModel*> WINDOW_TO_MODEL;

static char FirstDriveFromMask(ULONG unitmask) {
    char i;
    for (i = 0; i < 26; ++i) {
        if (unitmask & 0x1) {
            break;
        }
        unitmask = unitmask >> 1;
    }
    return(i + 'A');
}

LRESULT CALLBACK CddaDataModel::StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DEVICECHANGE: {
            if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
                DEV_BROADCAST_HDR* info = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);

                if (info->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                    DEV_BROADCAST_VOLUME* volumeInfo = reinterpret_cast<DEV_BROADCAST_VOLUME*>(lParam);

                    char driveLetter = FirstDriveFromMask(volumeInfo->dbcv_unitmask);
                    std::string drivePath = std::string(1, driveLetter) + ":";

                    if (GetDriveTypeA(drivePath.c_str()) == DRIVE_CDROM) {
                        auto it = WINDOW_TO_MODEL.find(hWnd);

                        if (it != WINDOW_TO_MODEL.end()) {
                            it->second->OnAudioDiscInsertedOrRemoved();
                            return 1;
                        }
                    }
                }
            }

            break;
        }

        case WM_CLOSE: {
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

CddaDataModel::CddaDataModel() {

}

CddaDataModel::~CddaDataModel() {
    this->StopWindowThread();
}

void CddaDataModel::WindowThreadProc() {
    HINSTANCE instance = (HINSTANCE) GetModuleHandle(nullptr);

    WNDCLASSW wndClass;
    wndClass.style = CS_DBLCLKS;
    wndClass.lpfnWndProc = CddaDataModel::StaticWndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = instance;
    wndClass.hIcon = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wndClass)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    RECT rc;
    SetRect(&rc, -3200, -3200, 32, 32);
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

    HWND hwnd = CreateWindowW(
        CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        (rc.right - rc.left),
        (rc.bottom - rc.top),
        0,
        NULL,
        instance,
        0);

    if (hwnd) {
        this->messageWindow = hwnd;
        WINDOW_TO_MODEL[hwnd] = this;

        ShowWindow(hwnd, SW_HIDE);

        MSG msg;
        msg.message = WM_NULL;

        while (::GetMessage(&msg, NULL, 0, 0) > 0) {
            if (msg.message == WM_QUIT) {
                ::PostQuitMessage(0);
                break;
            }

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        WINDOW_TO_MODEL.erase(WINDOW_TO_MODEL.find(hwnd));
    }

    DestroyWindow(hwnd);
    UnregisterClass(CLASS_NAME, NULL);
    this->messageWindow = nullptr;
}

void CddaDataModel::StartWindowThread() {
    Lock lock(this->windowMutex);

    if (!this->windowThread) {
        this->windowThread.reset(new std::thread(
            std::bind(&CddaDataModel::WindowThreadProc, this)));
    }
}

void CddaDataModel::StopWindowThread() {
    Lock lock(this->windowMutex);

    if (this->windowThread) {
        PostMessage(this->messageWindow, WM_QUIT, 0, 0);
        this->windowThread->join();
        this->windowThread.reset();
    }
}

std::vector<AudioDiscPtr> CddaDataModel::GetAudioDiscs() {
    std::vector<AudioDiscPtr> result;

    for (int i = 0; i < 26; i++) {
        char driveLetter = (char)('A' + i);
        std::string drive = std::string(1, driveLetter) + ":";

        AudioDiscPtr audioDisc = GetAudioDisc(driveLetter);
        if (audioDisc && audioDisc->GetTrackCount() > 0) {
            result.push_back(audioDisc);
        }
    }

    return result;
}

AudioDiscPtr CddaDataModel::GetAudioDisc(char driveLetter) {
    std::string drive = std::string(1, driveLetter) + ":";

    if (GetDriveTypeA(drive.c_str()) == DRIVE_CDROM) {
        drive = "\\\\.\\" + std::string(1, driveLetter) + ":";

        HANDLE driveHandle = CreateFileA(
            drive.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
            (HANDLE) nullptr);

        if (driveHandle == INVALID_HANDLE_VALUE) {
            return AudioDiscPtr();
        }

        CDROM_TOC toc = { 0 };
        DWORD byteCount = 0;

        BOOL readable = DeviceIoControl(
            driveHandle,
            IOCTL_CDROM_READ_TOC,
            NULL,
            0,
            &toc,
            sizeof(toc),
            &byteCount,
            0);

        CloseHandle(driveHandle);

        if (readable) {
            auto audioDisc = std::shared_ptr<AudioDisc>(new AudioDisc(driveLetter));

            int audioTrackCount = 0;
            int trackCount = (toc.LastTrack - toc.FirstTrack) + 1;
            DiscTrack::Type type;

            for (int j = 0; j < trackCount; j++) {
                type = DiscTrack::Type::Audio;

                if (toc.TrackData[j].Control & 4) {
                    type = DiscTrack::Type::Data;
                }
                else {
                    ++audioTrackCount;
                }

                auto start = MSF2UINT(toc.TrackData[j].Address) - FRAMES_PER_PREGAP;
                auto end = MSF2UINT(toc.TrackData[j + 1].Address) - FRAMES_PER_PREGAP;
                auto length = (end - start) * BYTES_PER_SECTOR;
                double duration = (double)(length / 2 / sizeof(short)) / 44100.0f;

                audioDisc->AddTrack(std::make_shared<DiscTrack>(
                    toc.TrackData[j], driveLetter, type, j, duration));
            }

            if (audioTrackCount > 0) {
                audioDisc->SetLeadout(std::make_shared<DiscTrack>(
                    toc.TrackData[trackCount], driveLetter, DiscTrack::Type::Leadout, trackCount + 1, 0));

                return audioDisc;
            }
        }
    }

    return AudioDiscPtr();
}

void CddaDataModel::AddEventListener(EventListener* listener) {
    Lock lock(eventListMutex);
    this->listeners.insert(listener);
}

void CddaDataModel::RemoveEventListener(EventListener* listener) {
    Lock lock(eventListMutex);
    auto it = this->listeners.find(listener);
    if (it != this->listeners.end()) {
        this->listeners.erase(it);
    }
}

void CddaDataModel::OnAudioDiscInsertedOrRemoved() {
    Lock lock(eventListMutex);
    for (auto l : this->listeners) {
        l->OnAudioDiscInsertedOrRemoved();
    }
}

/* * * * CddaDataModel::DiscTrack * * * */

CddaDataModel::DiscTrack::DiscTrack(
    TRACK_DATA& data,
    const char driveLetter,
    DiscTrack::Type type,
    int number,
    double duration)
{
    this->driveLetter = driveLetter;
    this->type = type;
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

std::string CddaDataModel::AudioDisc::GetCddbQueryString() {
    std::string query = "cmd=cddb+query+" + this->GetCddbId() + "+";
    query += std::to_string(this->GetTrackCount());

    /* tracks */
    for (auto track : this->tracks) {
        size_t frames =
            track->GetFrames() +
            (track->GetSeconds() * FRAMES_PER_SECOND) +
            (track->GetMinutes()) * FRAMES_PER_MINUTE;

        query += "+" + std::to_string(frames);
    }

    size_t leadoutOffset =
        leadout->GetFrames() +
        (leadout->GetSeconds() * FRAMES_PER_SECOND) +
        (leadout->GetMinutes()) * FRAMES_PER_MINUTE;

    query += "+" + std::to_string(leadoutOffset / FRAMES_PER_SECOND);

    return query;
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