//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 team
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

//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{ 
	namespace Query{
		class ListBase;
	}
	namespace tracklist{
        class IRandomAccess;
		typedef boost::shared_ptr<IRandomAccess> Ptr;
	} 
} }
//////////////////////////////////////////////////////////////////////////////

#include <core/tracklist/IBase.h>
#include <core/Library/Base.h>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <sigslot/sigslot.h>

namespace musik{ namespace core{
    namespace tracklist {
        class IRandomAccess : public IBase{
            public:
                ~IRandomAccess(void){};

                virtual void SetCurrentPosition(int position) = 0;
                virtual int CurrentPosition() = 0;

                virtual int Size() = 0;

                virtual musik::core::TrackPtr operator [](int position) = 0;
                virtual musik::core::TrackPtr TrackWithMetadata(int position)=0;

                virtual void SetLibrary(musik::core::LibraryPtr setLibrary) = 0;
                virtual musik::core::LibraryPtr Library() = 0;

                virtual bool CopyTracks(musik::core::tracklist::Ptr tracklist) = 0;
                virtual bool AppendTracks(musik::core::tracklist::Ptr tracklist) = 0;

                virtual void AddRequestedMetakey(const char* metakey) = 0;
                virtual void RemoveRequestedMetakey(const char* metakey) = 0;

                virtual UINT64 Duration() = 0;
                virtual UINT64 Filesize() = 0;

				virtual void HintNumberOfRows(int rows) = 0;
                virtual void ConnectToQuery(musik::core::Query::ListBase &listQuery)=0;

                /////////////////////////////////////////////////////////////////////////
                typedef sigslot::signal3<UINT64,UINT64,UINT64> TracklistInfoEvent;
                TracklistInfoEvent TracklistInfoUpdated;

                typedef sigslot::signal1<bool> TracksEvent;
                TracksEvent TracksUpdated;

                typedef sigslot::signal1<std::vector<int>&> TrackMetaEvent;
                TrackMetaEvent TrackMetaUpdated;

                typedef sigslot::signal2<int,int> PositionChangedEvent;
                PositionChangedEvent PositionChanged;
        };

        typedef boost::shared_ptr<IRandomAccess> Ptr;
		typedef boost::weak_ptr<IRandomAccess> WeakPtr;

    }
} }



