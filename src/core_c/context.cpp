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

#include "musikcore_c.h"

// #include <core/sdk/IResource.h>
// #include <core/sdk/IValue.h>
// #include <core/sdk/IValueList.h>
// #include <core/sdk/IMap.h>
// #include <core/sdk/IMapList.h>
// #include <core/sdk/ITrack.h>
// #include <core/sdk/ITrackList.h>
// #include <core/sdk/ITrackListEditor.h>
// #include <core/sdk/IMetadataProxy.h>

// using namespace musik::core::sdk;

// #define R(x) reinterpret_cast<IResource*>(x)
// #define V(x) reinterpret_cast<IValue*>(x)
// #define M(x) reinterpret_cast<IMap*>(x)
// #define VL(x) reinterpret_cast<IValueList*>(x)
// #define ML(x) reinterpret_cast<IMapList*>(x)
// #define TRACK(x) reinterpret_cast<ITrack*>(x)
// #define TRACKLIST(x) reinterpret_cast<ITrackList*>(x)
// #define TRACKLISTEDITOR(x) reinterpret_cast<ITrackListEditor*>(x)
// #define METADATA(x) reinterpret_cast<IMetadataProxy*>(x)

mcsdk_export bool mcsdk_context_init(mcsdk_context* context) {
    return false;
}

mcsdk_export bool mcsdk_context_release(mcsdk_context* context) {
    return false;
}
