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

#include <core/config.h>
#include <core/audio/Stream.h>
#include <core/sdk/IOutput.h>
#include <core/sdk/IPlayer.h>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace audio {

    class  Player;
    class  Transport;
    typedef boost::shared_ptr<Player> PlayerPtr;

    class Player : public IPlayer {
        public:
            typedef boost::shared_ptr<IOutput> OutputPtr;

            static PlayerPtr Create(std::string &url,OutputPtr *output=&OutputPtr());
    
        private:
            Player(std::string &url,OutputPtr *output);

        public:
            ~Player(void);

            virtual void OnBufferProcessedByOutput(IBuffer *buffer);

            void Play();
            void Stop();
            void Pause();
            void Resume();

            double Position();
            void SetPosition(double seconds);

            double Volume();
            void SetVolume(double volume);

            bool Exited();

        public:
            typedef sigslot::signal1<Player*> PlayerEvent;
            PlayerEvent PlaybackStarted;
            PlayerEvent PlaybackAlmostEnded;
            PlayerEvent PlaybackEnded;
            PlayerEvent PlaybackError;

            OutputPtr output;

        private:
            void ThreadLoop();
            bool PreBuffer();
            int State();
            void ReleaseAllBuffers();

        protected:
            friend class Transport;
            std::string url;

        private:
            typedef boost::scoped_ptr<boost::thread> ThreadPtr;
            typedef std::list<BufferPtr> BufferList;
            typedef std::set<BufferPtr> BufferSet;

            typedef enum {
                Precache = 0,
                Playing = 1,
                Quit = 2
            } States;

            StreamPtr stream;
            ThreadPtr thread;
            BufferList lockedBuffers;

            BufferList prebufferQueue;
            long prebufferSizeBytes;
            long maxPrebufferSizeBytes;

            boost::mutex mutex;
            boost::condition writeToOutputCondition;

            double volume;
            double currentPosition;
            double setPosition;
            int state;
    };

} } }

