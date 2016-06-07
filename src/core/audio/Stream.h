//////////////////////////////////////////////////////////////////////////////
// Copyright � 2007, Daniel �nnerby
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
#include <core/io/DataStreamFactory.h>
#include <core/audio/Buffer.h>
#include <core/sdk/IDecoder.h>
#include <core/sdk/IDSP.h>
#include <core/sdk/IDecoderFactory.h>

#include <boost/shared_ptr.hpp>
#include <list>

namespace musik { namespace core { namespace audio {

    class Stream;
    class Player;
    typedef std::shared_ptr<Stream> StreamPtr;

    class Stream {
        public:
            static StreamPtr Create(unsigned int options=0);

            typedef enum {
                NoDSP = 1
            } Options;

        private:
            Stream(unsigned int options);

        public:
            ~Stream(void);

            BufferPtr GetNextProcessedOutputBuffer();
            void OnBufferProcessedByPlayer(BufferPtr buffer);
            double SetPosition(double seconds);
            bool OpenStream(std::string uri);
            double DecoderProgress();

        private:
            void RecycleBuffer(BufferPtr oldBuffer);
            BufferPtr GetNextBufferFromDecoder();
            BufferPtr GetEmptyBuffer();
            void LoadDecoderPlugins();

            typedef std::list<BufferPtr> BufferList;
            typedef std::shared_ptr<IDecoderFactory> DecoderFactoryPtr;
            typedef std::vector<DecoderFactoryPtr> DecoderFactoryList;
            typedef std::shared_ptr<IDecoder> DecoderPtr;
            typedef std::shared_ptr<IDSP> DspPtr;
            typedef std::vector<DspPtr> Dsps;

            long preferedBufferSampleSize;
            double maxCacheLength;
            unsigned int options;
            long decoderSampleRate;
            uint64 decoderSamplePosition;
            std::string uri;
            musik::core::io::DataStreamFactory::DataStreamPtr dataStream;
            BufferList recycledBuffers;

            DecoderFactoryList decoderFactories;
            DecoderPtr decoder;
            Dsps dsps;
    };

} } }
