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
#pragma once

#include <musikcore/config.h>
#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IDataStreamFactory.h>
#include <vector>
#include <functional>

namespace musik { namespace core { namespace io {

    /* call after stream users stop, before plugin modules unload. */
    void Shutdown();

    class DataStreamFactory {
        public:
            using DataStreamPtr = std::shared_ptr<musik::core::sdk::IDataStream>;
            using OpenFlags = musik::core::sdk::OpenFlags;

            /* publish every stream before Open so cancellation can reach its
            startup work. false stops the attempt without opening it. */
            static DataStreamPtr OpenSharedDataStream(const char *uri, OpenFlags flags,
                std::function<bool(const DataStreamPtr&)> publish = {});
            /* caller owns the unopened stream and must Release it. */
            static musik::core::sdk::IDataStream* CreateDataStream(const char* uri);

        private:
            using DataStreamFactoryVector = std::vector<std::shared_ptr<musik::core::sdk::IDataStreamFactory>>;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(DataStreamFactory)

            DataStreamFactory();
            static std::shared_ptr<DataStreamFactory> Instance();

            DataStreamFactoryVector dataStreamFactories;
    };

} } }
