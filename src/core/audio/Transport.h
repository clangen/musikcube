//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
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

#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <sigslot/sigslot.h>

#include <core/config.h>

namespace musik { namespace core { namespace audio {

class AudioStream;
class IAudioSourceSupplier;
class IAudioOutputSupplier;

typedef std::vector<utfstring> AudioStreamOverview;
typedef std::vector<utfstring>::const_iterator AudioStreamOverviewIterator;

///\brief
///Transport is the API to the audio engine.  It keeps track of the active audio streams,
///input and output.
///
///It takes care of finding the right decoder for a given file or URL.
///
///It can handle multiple streams to deal with crossfading.  The active stream is always the first stream.
class Transport
{
public:     
    Transport();
    ~Transport();

    ///\brief Start a new audiostream based on a file path or URL.
    void            Start(const utfstring path);
    ///\brief Stop the active stream.  All resources used are released.
    void            Stop();
    ///\brief Pause the active stream.  All resources stay in use.
    bool            Pause();
    ///\brief Resume the active stream
    bool            Resume();

    ///\brief Jump to a given position in the active stream.
    ///\param position New position in milliseconds
    void            SetTrackPosition(unsigned long position);
    ///\brief Get current position in active stream
    ///\return Current position in milliseconds
    unsigned long   TrackPosition() const;
    ///\brief Get length of active stream.
    ///\return Length in in milliseconds.  Can be 0.
    unsigned long   TrackLength()   const;

    ///\brief Set the volume for the active stream
    ///\param volume New volume level in percent (0-100)
    void            SetVolume(short volume);
    ///\brief Get the current volume level
    ///\return Current volume as a percentage (0-100)
    short           Volume() const { return currVolume; };

    ///\brief Get the number of open streams
    size_t          NumOfStreams()  const;
    ///\brief Get a list with descriptions of the open streams
    AudioStreamOverview	StreamsOverview()	const;

private: 
    typedef std::vector<boost::shared_ptr<IAudioSourceSupplier> > SourceSupplierList;
    SourceSupplierList  registeredSourceSuppliers;
    typedef std::vector<boost::shared_ptr<IAudioOutputSupplier> > OutputSupplierList;
    OutputSupplierList  registeredOutputSuppliers;

    std::vector<AudioStream*> openStreams;

    AudioStream*    CreateStream(const utfstring sourceString);
    void            RemoveFinishedStreams();

    short currVolume;

// Signals
public:  
    ///\brief Emitted when Start() completed successfully
    sigslot::signal0<>  EventPlaybackStartedOk;
    ///\brief Emitted when Start() failed
    sigslot::signal0<>  EventPlaybackStartedFail;
    ///\brief Emitted when Stop() completed successfully
    sigslot::signal0<>  EventPlaybackStoppedOk;
    ///\brief Emitted when Stop() failed
    sigslot::signal0<>  EventPlaybackStoppedFail;

    ///\brief Emitted when Pause() completed successfully
    sigslot::signal0<>  EventPlaybackPausedOk;
    ///\brief Emitted when Pause() failed
    sigslot::signal0<>  EventPlaybackPausedFail;
    ///\brief Emitted when Resume() completed successfully
    sigslot::signal0<>  EventPlaybackResumedOk;
    ///\brief Emitted when Resume() failed
    sigslot::signal0<>  EventPlaybackResumedFail;

    ///\brief Emitted when SetVolume() completed successfully
    sigslot::signal0<>  EventVolumeChangedOk;
    ///\brief Emitted when SetVolume() failed
    sigslot::signal0<>  EventVolumeChangedFail;

    ///\brief Emitted when crossfading can start.  The PlaybackQueue should offer the next track.  
    sigslot::signal0<>  EventMixpointReached;
};

}}} // NS
