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

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <set>
#include "ntddcdrm.h"
#include "devioctl.h"

class CddaDataModel {
    public:
        class EventListener {
            public:
                virtual void OnAudioDiscInsertedOrRemoved() = 0;
        };

        struct DiscTrack {
            public:
                enum class Type { Audio, Data, Leadout };

                DiscTrack(TRACK_DATA& data, char driveLetter, Type type, int number, double duration);

                int GetCddbSum();

                int GetNumber() { return this->number; }
                int GetMinutes() { return this->minutes; }
                int GetSeconds() { return this->seconds; }
                int GetFrames() { return this->frames; }
                Type GetType() { return this->type; }
                double GetDuration() { return duration; }

                std::string GetFilePath();

            private:
                double duration;
                Type type;
                char driveLetter;
                int number;
                int minutes;
                int seconds;
                int frames;
        };

        using DiscTrackPtr = std::shared_ptr<DiscTrack>;

        class AudioDisc {
            public:
                AudioDisc(char driveLetter);

                std::string GetCddbId();
                std::string GetCddbQueryString();

                void SetLeadout(DiscTrackPtr leadout);
                void AddTrack(DiscTrackPtr track);
                int GetTrackCount();
                DiscTrackPtr GetTrackAt(int index);
                char GetDriveLetter() { return this->driveLetter; }

            private:
                std::vector<DiscTrackPtr> tracks;
                DiscTrackPtr leadout;
                char driveLetter;
        };

        using AudioDiscPtr = std::shared_ptr<AudioDisc>;

        static CddaDataModel& Instance() {
            return Instance(true);
        }

        static void Shutdown() {
            Instance(false).StopWindowThread();
        }

        CddaDataModel& operator=(const CddaDataModel&) = delete;
        CddaDataModel(const CddaDataModel&) = delete;

        std::vector<AudioDiscPtr> GetAudioDiscs();
        AudioDiscPtr GetAudioDisc(char driveLetter);

        void AddEventListener(EventListener* listener);
        void RemoveEventListener(EventListener* listener);

    private:
        static CddaDataModel& Instance(bool start) {
            static CddaDataModel model;
            if (start) {
                model.StartWindowThread();
            }
            return model;
        }

        using Thread = std::shared_ptr<std::thread>;
        using Mutex = std::recursive_mutex;
        using Lock = std::unique_lock<Mutex>;

        CddaDataModel();
        ~CddaDataModel();

        void StartWindowThread();
        void StopWindowThread();
        void WindowThreadProc();
        void OnAudioDiscInsertedOrRemoved();

        static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        Mutex windowMutex, eventListMutex;
        Thread windowThread;
        HWND messageWindow;
        std::set<EventListener*> listeners;
};