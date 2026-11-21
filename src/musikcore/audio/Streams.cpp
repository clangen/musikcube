//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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
#include <musikcore/debug.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include <musikcore/sdk/IEncoderFactory.h>
#include <musikcore/plugin/PluginFactory.h>
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

namespace {
    /* one provider generation is retained by each in-flight factory lookup. */
    struct FactoryCache {
        DecoderFactoryList decoders;
        EncoderFactoryList encoders;
    };

    std::mutex initLock;
    std::shared_ptr<const FactoryCache> factories;

    std::shared_ptr<const FactoryCache> getFactories() {
        std::lock_guard<std::mutex> lock(initLock);
        if (!factories) {
            const auto cache = std::make_shared<FactoryCache>();
            cache->decoders = PluginFactory::Instance()
                .QueryInterface<IDecoderFactory, PluginFactory::ReleaseDeleter<IDecoderFactory>>("GetDecoderFactory");
            cache->encoders = PluginFactory::Instance()
                .QueryInterface<IEncoderFactory, PluginFactory::ReleaseDeleter<IEncoderFactory>>("GetEncoderFactory");
            factories = cache;
        }
        return factories;
    }
}

namespace musik { namespace core { namespace audio {

    namespace streams {
        void Shutdown() {
            std::shared_ptr<const FactoryCache> retired;
            {
                std::lock_guard<std::mutex> lock(initLock);
                retired.swap(factories);
            }
            /* provider Release may reenter core; do not hold the cache mutex. */
        }

        IDecoder* GetDecoderForDataStream(IDataStream* dataStream) {
            const auto cache = getFactories();

            IDecoder* decoder = nullptr;

            /* find a DecoderFactory we can use for this type of data */
            auto factories = cache->decoders.begin();
            const auto end = cache->decoders.end();
            std::shared_ptr<IDecoderFactory> factory;

            for (; factories != end && !factory; ++factories) {
                if ((*factories)->CanHandle(dataStream->Type())) {
                    factory = (*factories);
                    break;
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
            const auto cache = getFactories();

            for (const auto& factory : cache->encoders) {
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