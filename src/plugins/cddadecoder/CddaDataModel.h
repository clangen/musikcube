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
/// @file CddaDataModel.h
/// @brief Models audio CDs and their tracks on Windows.
/// @details Provides a singleton that enumerates audio CDs present in the
/// system, exposes per-disc track information (including CDDB ids for metadata
/// lookup), and monitors drive insertion/removal events through a hidden
/// message window. Windows-only.
///

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <set>

/** @brief Singleton model of the audio discs available on the system.
 *  @details Maintains a list of AudioDisc objects, one per CD drive containing
 *  an audio disc, and notifies registered listeners when discs are inserted
 *  or removed. */
class CddaDataModel {
    public:
        /** @brief Callback interface for CD insertion/removal notifications. */
        class EventListener {
            public:
                /** @brief Called when an audio disc is inserted or removed. */
                virtual void OnAudioDiscInsertedOrRemoved() = 0;
        };

        /** @brief Represents a single track on an audio CD.
         *  @details Tracks are either audio data, plain data sectors, or the
         *  leadout (end-of-disc) marker. */
        struct DiscTrack {
            public:
                /** @brief Category of a CD track. */
                enum class Type { Audio, Data, Leadout };

                /** @brief Constructs a track from a raw TOC entry.
                 *  @param data Raw CDROM_TOC track descriptor.
                 *  @param driveLetter Drive letter the disc is mounted on.
                 *  @param type Track type (audio/data/leadout).
                 *  @param number 1-based track number.
                 *  @param duration Track duration in seconds. */
                DiscTrack(TRACK_DATA& data, char driveLetter, Type type, int number, double duration);

                /** @brief Computes the CDDB disc-id contribution for this track.
                 *  @return The track's CDDB sum value. */
                int GetCddbSum();

                /** @brief Returns the 1-based track number. @return Track number. */
                int GetNumber() noexcept { return this->number; }
                /** @brief Returns the track length in whole minutes. */
                int GetMinutes() noexcept { return this->minutes; }
                /** @brief Returns the track length in seconds. */
                int GetSeconds() noexcept { return this->seconds; }
                /** @brief Returns the track length in CD frames. */
                int GetFrames() noexcept { return this->frames; }
                /** @brief Returns the track type. @return The track type. */
                Type GetType() noexcept { return this->type; }
                /** @brief Returns the track duration in seconds. */
                double GetDuration() noexcept { return duration; }

                /** @brief Builds the plugin URI identifying this track.
                 *  @return A string like "cdda://D:/track/3". */
                std::string GetFilePath();

            private:
                /** @brief Track duration in seconds. */
                double duration;
                /** @brief Track type (audio/data/leadout). */
                Type type;
                /** @brief Drive letter of the disc. */
                char driveLetter;
                /** @brief 1-based track number. */
                int number;
                /** @brief Length expressed in minutes/seconds/frames. */
                int minutes;
                int seconds;
                int frames;
        };

        /** @brief Shared pointer to a DiscTrack. */
        using DiscTrackPtr = std::shared_ptr<DiscTrack>;

        /** @brief Represents an audio CD mounted in a drive.
         *  @details Holds the ordered list of audio tracks plus the disc
         *  leadout, and can generate CDDB lookup identifiers. */
        class AudioDisc {
            public:
                /** @brief Constructs a disc for the given drive letter.
                 *  @param driveLetter Letter of the drive containing the disc. */
                AudioDisc(char driveLetter);

                /** @brief Returns the CDDB disc id.
                 *  @return The 8-hex-digit CDDB disc id string. */
                std::string GetCddbId();
                /** @brief Returns a CDDB query string for metadata lookups.
                 *  @return The CDDB "query" payload for this disc. */
                std::string GetCddbQueryString();

                /** @brief Sets the disc leadout track.
                 *  @param leadout The leadout track. */
                void SetLeadout(DiscTrackPtr leadout);
                /** @brief Appends an audio track to the disc.
                 *  @param track The track to add. */
                void AddTrack(DiscTrackPtr track);
                /** @brief Returns the number of audio tracks.
                 *  @return Track count. */
                int GetTrackCount();
                /** @brief Returns the track at the given index.
                 *  @param index Zero-based track index.
                 *  @return The requested track, or null. */
                DiscTrackPtr GetTrackAt(int index);
                /** @brief Returns the drive letter. @return Drive letter. */
                char GetDriveLetter() noexcept { return this->driveLetter; }

            private:
                /** @brief Ordered list of audio tracks. */
                std::vector<DiscTrackPtr> tracks;
                /** @brief The leadout track. */
                DiscTrackPtr leadout;
                /** @brief Drive letter the disc is mounted on. */
                char driveLetter;
        };

        /** @brief Shared pointer to an AudioDisc. */
        using AudioDiscPtr = std::shared_ptr<AudioDisc>;

        /** @brief Returns the singleton instance, starting the monitor thread.
         *  @return Reference to the shared model. */
        static CddaDataModel& Instance() {
            return Instance(true);
        }

        /** @brief Shuts down the model and its monitor thread. */
        static void Shutdown() {
            Instance(false).StopWindowThread();
        }

        CddaDataModel& operator=(const CddaDataModel&) = delete;
        CddaDataModel(const CddaDataModel&) = delete;

        /** @brief Returns all currently mounted audio discs.
         *  @return Vector of shared disc pointers. */
        std::vector<AudioDiscPtr> GetAudioDiscs();
        /** @brief Returns the audio disc in the given drive.
         *  @param driveLetter Drive letter to look up.
         *  @return The disc, or null if none is mounted. */
        AudioDiscPtr GetAudioDisc(char driveLetter);

        /** @brief Registers a listener for disc insertion/removal events.
         *  @param listener The listener to add. */
        void AddEventListener(EventListener* listener);
        /** @brief Unregisters a listener.
         *  @param listener The listener to remove. */
        void RemoveEventListener(EventListener* listener);

    private:
        /** @brief Returns the singleton, optionally starting its thread.
         *  @param start True to start the window thread.
         *  @return Reference to the shared model. */
        static CddaDataModel& Instance(bool start) {
            static CddaDataModel model;
            if (start) {
                model.StartWindowThread();
            }
            return model;
        }

        /** @brief Shared thread handle type. */
        using Thread = std::shared_ptr<std::thread>;
        /** @brief Recursive mutex used for internal locking. */
        using Mutex = std::recursive_mutex;
        /** @brief Lock guard type. */
        using Lock = std::unique_lock<Mutex>;

        CddaDataModel();
        ~CddaDataModel();

        /** @brief Starts the hidden window / monitoring thread. */
        void StartWindowThread();
        /** @brief Stops the hidden window / monitoring thread. */
        void StopWindowThread();
        /** @brief Message pump for the hidden monitoring window. */
        void WindowThreadProc();
        /** @brief Handles a disc insertion/removal notification. */
        void OnAudioDiscInsertedOrRemoved();

        /** @brief Static window procedure receiving device change messages. */
        static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        /** @brief Guards the window and listener list. */
        Mutex windowMutex, eventListMutex;
        /** @brief Handle of the monitoring window thread. */
        Thread windowThread;
        /** @brief Hidden window used to receive device change notifications. */
        HWND messageWindow;
        /** @brief Registered event listeners. */
        std::set<EventListener*> listeners;
};