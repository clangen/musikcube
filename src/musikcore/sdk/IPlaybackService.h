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

#include "constants.h"
#include "ITrack.h"
#include "ITrackList.h"
#include "ITrackListEditor.h"

namespace musik { namespace core { namespace sdk {

    class IPlaybackService {
        public:
            /* sdk v1 */
            virtual void Play(size_t index) = 0;
            virtual bool Next() = 0;
            virtual bool Previous() = 0;
            virtual void Stop() = 0;

            virtual musik::core::sdk::RepeatMode GetRepeatMode() = 0;
            virtual void SetRepeatMode(musik::core::sdk::RepeatMode mode) = 0;
            virtual void ToggleRepeatMode() = 0;

            virtual musik::core::sdk::PlaybackState GetPlaybackState() = 0;

            virtual bool IsShuffled() = 0;
            virtual void ToggleShuffle() = 0;
            virtual void PauseOrResume() = 0;

            virtual double GetVolume() = 0;
            virtual void SetVolume(double volume) = 0;

            virtual double GetPosition() = 0;
            virtual void SetPosition(double seconds) = 0;
            virtual double GetDuration() = 0;

            virtual bool IsMuted() = 0;
            virtual void ToggleMute() = 0;

            virtual size_t GetIndex() = 0;
            virtual size_t Count() = 0;

            virtual ITrack* GetTrack(size_t index) = 0;

            /* sdk v2 */
            virtual ITrack* GetPlayingTrack() = 0;

            /* sdk v3 */
            virtual void CopyFrom(const ITrackList* trackList) = 0;
            virtual void Play(const ITrackList* source, size_t index) = 0;
            virtual ITrackListEditor* EditPlaylist() = 0;

            /* sdk v5 */
            virtual musik::core::sdk::TimeChangeMode GetTimeChangeMode() = 0;
            virtual void SetTimeChangeMode(musik::core::sdk::TimeChangeMode) = 0;

            /* sdk v13 */
            virtual void ReloadOutput() = 0;
            virtual ITrackList* Clone() = 0;
    };

} } }

