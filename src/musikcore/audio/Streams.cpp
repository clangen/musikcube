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
#include <musikcore/sdk/IEncoderFactory.h>
#include <musikcore/plugin/PluginFactory.h>
#include <mutex>

using namespace musik::core::audio;
using namespace musik::core::sdk;
using musik::core::PluginFactory;

using EncoderFactoryList = std::vector<std::shared_ptr<IEncoderFactory>>;
using DspList = std::vector<std::shared_ptr<IDSP>>;

namespace {
    std::mutex initLock;
    std::shared_ptr<const EncoderFactoryList> factories;

    std::shared_ptr<const EncoderFactoryList> getFactories() {
        std::lock_guard<std::mutex> lock(initLock);
        if (!factories) {
            using Deleter = PluginFactory::ReleaseDeleter<IEncoderFactory>;
            factories = std::make_shared<EncoderFactoryList>(
                PluginFactory::Instance().QueryInterface<IEncoderFactory, Deleter>("GetEncoderFactory"));
        }
        return factories;
    }
}

namespace musik { namespace core { namespace audio {

    namespace streams {
        void Shutdown() {
            std::shared_ptr<const EncoderFactoryList> retired;
            {
                std::lock_guard<std::mutex> lock(initLock);
                retired.swap(factories);
            }
            /* provider Release may reenter core; do not hold the cache mutex. */
        }

        IEncoder* GetEncoderForType(const char* type) {
            const auto cache = getFactories();

            for (const auto& factory : *cache) {
                if (factory->CanHandle(type)) {
                    return factory->CreateEncoder(type);
                }
            }

            return nullptr;
        }

        DspList GetDspPlugins() {
            typedef PluginFactory::ReleaseDeleter<IDSP> Deleter;
            return PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
        }
    };

} } }