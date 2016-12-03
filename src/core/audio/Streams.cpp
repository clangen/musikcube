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

#include "pch.hpp"

#include "Streams.h"
#include <core/debug.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/plugin/PluginFactory.h>
#include <mutex>

#define TAG "Streams"

using namespace musik::core::audio;
using namespace musik::core::sdk;
using musik::core::PluginFactory;

using DataStreamPtr = musik::core::io::DataStreamFactory::DataStreamPtr;
using IDSP = musik::core::sdk::IDSP;
using IDecoder = musik::core::sdk::IDecoder;
using IDecoderFactory = musik::core::sdk::IDecoderFactory;
using DecoderFactoryList = std::vector<std::shared_ptr<IDecoderFactory > >;
using DspList = std::vector<std::shared_ptr<IDSP > >;

static std::mutex initLock;
static DecoderFactoryList decoders;

static void init() {
    std::unique_lock<std::mutex> lock(initLock);
    if (!decoders.size()) {
        typedef PluginFactory::DestroyDeleter<IDecoderFactory> Deleter;

        decoders = PluginFactory::Instance()
            .QueryInterface<IDecoderFactory, Deleter>("GetDecoderFactory");
    }
}

namespace musik { namespace core { namespace audio {

    namespace streams {
        std::shared_ptr<IDecoder> GetDecoderForDataStream(DataStreamPtr dataStream) {
            init();

            std::shared_ptr<IDecoder> result;

            /* find a DecoderFactory we can use for this type of data*/
            DecoderFactoryList::iterator factories = decoders.begin();
            DecoderFactoryList::iterator end = decoders.end();
            std::shared_ptr<IDecoderFactory> factory;

            for ( ; factories != end && !factory; ++factories) {
                if ((*factories)->CanHandle(dataStream->Type())) {
                    factory  = (*factories);
                }
            }

            const std::string uri = dataStream->Uri();

            if (!factory) {
                /* nothing can decode this type of file */
                musik::debug::err(TAG, "nothing could open " + uri);
                return result;
            }

            IDecoder *decoder = factory->CreateDecoder();
            if (!decoder) {
                /* shouldn't ever happen, the factory said it can handle this file */
                return result;
            }

            /* ask the decoder to open the data stream. if it returns true we're
            good to start pulling data out of it! */
            typedef PluginFactory::DestroyDeleter<IDecoder> Deleter;

            result.reset(decoder, Deleter());
            if (!result->Open(dataStream.get())) {
                musik::debug::err(TAG, "open ok, but decode failed " + uri);
                result.reset();
                return result;
            }

            musik::debug::info(TAG, "about ready to play: " + uri);

            return result;
        }

        DspList GetDspPlugins() {
            typedef PluginFactory::DestroyDeleter<IDSP> Deleter;
            return PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
        }
    };

} } }