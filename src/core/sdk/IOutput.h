//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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

#include "config.h"
#include "IDataStream.h"
#include "IBuffer.h"
#include "IPlayer.h"

namespace musik { namespace core { namespace audio {

    class IOutput {
        public:    
            //////////////////////////////////////////
            ///\brief
            ///Destroy the object
            ///
            ///The Destroy method is used so that it's guaranteed that the object is 
            ///destroyed inside the right DLL/exe
            //////////////////////////////////////////
            virtual void Destroy() = 0;

            //////////////////////////////////////////
            ///\brief
            ///Pause the current output
            //////////////////////////////////////////
            virtual void Pause() = 0;

            //////////////////////////////////////////
            ///\brief
            ///resume a paused output
            //////////////////////////////////////////
            virtual void Resume() = 0;

            //////////////////////////////////////////
            ///\brief
            ///Set the volume on this output
            //////////////////////////////////////////
            virtual void SetVolume(double volume) = 0;

            //////////////////////////////////////////
            ///\brief
            ///Clear internal buffers. Used when setting new position in a stream
            //////////////////////////////////////////
            virtual void Stop() = 0;

            //////////////////////////////////////////
            ///\brief
            ///Play this buffer
            //////////////////////////////////////////
            virtual bool Play(IBuffer *buffer, IPlayer *player) = 0;
    };

} } }
