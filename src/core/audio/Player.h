//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <core/sdk/IBufferProvider.h>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace audio {

    class Player;
    typedef std::shared_ptr<Player> PlayerPtr;

    class Output;
    typedef std::shared_ptr<IOutput> OutputPtr;

    class Player : public IBufferProvider {
        public:
            static OutputPtr CreateDefaultOutput();

            static PlayerPtr Create(
                const std::string &url,
                OutputPtr output);

            Player(
                const std::string &url,
                OutputPtr output);

            ~Player();

            virtual void OnBufferProcessed(IBuffer *buffer);

            void Play();
            void Stop();

            double Position();
            void SetPosition(double seconds);

            std::string GetUrl() const { return this->url; }

            bool Exited();

            typedef sigslot::signal1<Player*> PlayerEvent;
            PlayerEvent PlaybackStarted;
            PlayerEvent PlaybackAlmostEnded;
            PlayerEvent PlaybackFinished;
            PlayerEvent PlaybackError;

        private:
            void ThreadLoop();
            bool PreBuffer();
            int State();
            void ReleaseAllBuffers();

        private:
            typedef boost::scoped_ptr<boost::thread> ThreadPtr;
            typedef std::list<BufferPtr> BufferList;
            typedef std::set<BufferPtr> BufferSet;

            typedef enum {
                Precache = 0,
                Playing = 1,
                Quit = 2
            } States;

            OutputPtr output;
            StreamPtr stream;
            ThreadPtr thread;
            BufferList lockedBuffers;

            std::string url;

            BufferList prebufferQueue;

            boost::mutex queueMutex, positionMutex;
            boost::condition writeToOutputCondition;

            double volume;
            double currentPosition;
            double setPosition;
            int state;
            bool notifiedStarted;
    };

} } }
