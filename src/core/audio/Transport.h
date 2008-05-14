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

class Transport
{
public:     Transport();
public:     ~Transport();

public:     void            Start(const utfstring path);
public:     void            Stop(size_t idx);

public:     void            JumpToPosition(short relativePosition);

public:     void            ChangeVolume(short volume);
public:     short           Volume() const { return currVolume; };

public:     size_t				NumOfStreams()		const;
public:		AudioStreamOverview	StreamsOverview()	const;

private:    AudioStream*    CreateStream(const utfstring sourceString);
private:    void            RemoveFinishedStreams();

private:    typedef std::vector<boost::shared_ptr<IAudioSourceSupplier> > SourceSupplierList;
private:    SourceSupplierList  registeredSourceSuppliers;
private:    typedef std::vector<boost::shared_ptr<IAudioOutputSupplier> > OutputSupplierList;
private:    OutputSupplierList  registeredOutputSuppliers;

private:    std::vector<AudioStream*> openStreams;

private:    short currVolume;

// Signals
public:     sigslot::signal0<>  PlaybackStartedOk;
public:     sigslot::signal0<>  PlaybackStartedFail;
public:     sigslot::signal0<>  PlaybackStoppedOk;
public:     sigslot::signal0<>  PlaybackStoppedFail;
public:     sigslot::signal0<>  VolumeChangedOk;
public:     sigslot::signal0<>  VolumeChangedFail;
public:     sigslot::signal0<>  MixpointReached;  // TODO: For crossfading.  Consider renaming.
/* Possible other signals
public:     sigslot::signal0<>  PlaybackInterrupted;
public:     sigslot::signal0<>  StreamOpenOk;
public:     sigslot::signal0<>  StreamOpenFail;
public:     sigslot::signal0<>  SetPositionOk;
public:     sigslot::signal0<>  SetPositionFail;
*/
};

}}} // NS
