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

#include "pch.hpp"

#include "Streams.h"
#include <core/debug.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/sdk/IEncoderFactory.h>
#include <core/plugin/PluginFactory.h>
#include <mutex>

#define TAG "Streams"

using namespace musik::core::audio;
using namespace musik::core::sdk;
using musik::core::PluginFactory;

using DataStreamPtr = musik::core::io::DataStreamFactory::DataStreamPtr;
using DecoderFactoryList = std::vector<std::shared_ptr<IDecoderFactory>>;
using EncoderFactoryList = std::vector<std::shared_ptr<IEncoderFactory>>;
using DspList = std::vector<std::shared_ptr<IDSP>>;
using Deleter = PluginFactory::ReleaseDeleter<IDecoder>;
using DecoderPtr = std::shared_ptr<IDecoder>;

static std::mutex initLock;
static DecoderFactoryList decoders;
static EncoderFactoryList encoders;

static void init() {
    std::unique_lock<std::mutex> lock(initLock);

    if (!decoders.size()) {
        using Deleter = PluginFactory::ReleaseDeleter<IDecoderFactory>;

        decoders = PluginFactory::Instance()
            .QueryInterface<IDecoderFactory, Deleter>("GetDecoderFactory");
    }

    if (!encoders.size()) {
        using Deleter = PluginFactory::ReleaseDeleter<IEncoderFactory>;

        encoders = PluginFactory::Instance()
            .QueryInterface<IEncoderFactory, Deleter>("GetEncoderFactory");
    }
}

namespace musik { namespace core { namespace audio {

    namespace streams {
        IDecoder* GetDecoderForDataStream(IDataStream* dataStream) {
            init();

            IDecoder* decoder = nullptr;

            /* find a DecoderFactory we can use for this type of data*/
            DecoderFactoryList::iterator factories = decoders.begin();
            DecoderFactoryList::iterator end = decoders.end();
            std::shared_ptr<IDecoderFactory> factory;

            for (; factories != end && !factory; ++factories) {
                if ((*factories)->CanHandle(dataStream->Type())) {
                    factory = (*factories);
                }
            }

            const std::string uri = dataStream->Uri();

            if (!factory) {
                /* nothing can decode this type of file */
                musik::debug::error(TAG, "nothing could open " + uri);
                return nullptr;
            }

            decoder = factory->CreateDecoder();

            if (!decoder) {
                /* shouldn't ever happen, the factory said it can handle this file */
                return nullptr;
            }

            /* ask the decoder to open the data stream. if it returns true we're
            good to start pulling data out of it! */
            if (!decoder->Open(dataStream)) {
                musik::debug::error(TAG, "open ok, but decode failed " + uri);
                decoder->Release();
                return nullptr;
            }

            musik::debug::info(TAG, "found a decoder for " + uri);

            return decoder;
        }

        IEncoder* GetEncoderForType(const char* type) {
            init();

            for (auto factory : encoders) {
                if (factory->CanHandle(type)) {
                    return factory->CreateEncoder(type);
                }
            }

            return nullptr;
        }

        std::shared_ptr<IDecoder> GetDecoderForDataStream(DataStreamPtr dataStream) {
            auto decoder = GetDecoderForDataStream(dataStream.get());
            return decoder ? DecoderPtr(decoder, Deleter()) : DecoderPtr();
        }

        DspList GetDspPlugins() {
            typedef PluginFactory::ReleaseDeleter<IDSP> Deleter;
            return PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
        }
    };

} } }