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
#include <core/filestreams/Factory.h>
#include <core/audio/Buffer.h>
#include <core/audio/IDecoder.h>
#include <core/audio/IDSP.h>
#include <core/audio/IDecoderFactory.h>

#include <boost/shared_ptr.hpp>
//#include <boost/thread/mutex.hpp>
#include <list>

//////////////////////////////////////////////////////////////////////////////
namespace musik { namespace core { namespace audio {
//////////////////////////////////////////////////////////////////////////////

// Forward declare
class Stream;
class Player;
typedef boost::shared_ptr<Stream> StreamPtr;

//////////////////////////////////////////////////////////////////////////////
class Stream {
    public:
        static StreamPtr Create(unsigned int options=0);

        enum Options:unsigned int{
            NoDSP = 1
        };

    private:
        Stream(unsigned int options);
    public:
        ~Stream(void);

        BufferPtr NextBuffer();
        bool PreCache();

        double SetPosition(double seconds);

//        void SetMaxCacheLength(double seconds=1.5);
 //       void SetPreferedBufferSampleSize(long samples);

        bool OpenStream(utfstring uri);
        double DecoderProgress();
        

    private:
        BufferPtr GetNextDecoderBuffer();
        BufferPtr GetNextBuffer();

    private:
        friend class Player;
        BufferPtr NewBuffer();
        void DeleteBuffer(BufferPtr oldBuffer);

    private:        
        long preferedBufferSampleSize;
        double maxCacheLength;
        unsigned int options;

        long decoderSampleRate;
        UINT64 decoderSamplePosition;

        utfstring uri;
        musik::core::filestreams::FileStreamPtr fileStream;

        typedef std::list<BufferPtr> BufferList;
        BufferList availableBuffers;
        
//        boost::mutex bufferMutex;

        /////////////////////////////
        // Decoder stuff
        typedef boost::shared_ptr<IDecoder> DecoderPtr;
        DecoderPtr decoder;

        /////////////////////////////
        // DSP Stuff
        typedef boost::shared_ptr<IDSP> DspPtr;
        typedef std::vector<DspPtr> Dsps;
        Dsps dsps;

        // Helper with decoder factories
        class StreamHelper{
            public:
                typedef boost::shared_ptr<IDecoderFactory> DecoderFactoryPtr;
                typedef std::vector<DecoderFactoryPtr> DecoderFactories;
                DecoderFactories decoderFactories;

                StreamHelper();

        };

        typedef boost::shared_ptr<StreamHelper> StreamHelperPtr;
        static StreamHelperPtr Helper();

};

//////////////////////////////////////////////////////////////////////////////
} } }
//////////////////////////////////////////////////////////////////////////////

