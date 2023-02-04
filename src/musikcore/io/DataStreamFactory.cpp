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

#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/config.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/io/LocalFileStream.h>

using namespace musik::core::io;
using namespace musik::core::sdk;

using DataStreamPtr = DataStreamFactory::DataStreamPtr;
using StreamDeleter = musik::core::PluginFactory::ReleaseDeleter<IDataStream>;

DataStreamFactory::DataStreamFactory() {
    typedef IDataStreamFactory PluginType;
    typedef musik::core::PluginFactory::ReleaseDeleter<PluginType> Deleter;

    this->dataStreamFactories = musik::core::PluginFactory::Instance()
        .QueryInterface<PluginType, Deleter>("GetDataStreamFactory");
}

DataStreamFactory* DataStreamFactory::Instance() {
    static DataStreamFactory* instance = NULL;

    if (instance == NULL) {
        instance = new DataStreamFactory();
    }

    return instance;
}

IDataStream* DataStreamFactory::OpenDataStream(const char* uri, OpenFlags flags) {
    typedef musik::core::PluginFactory::ReleaseDeleter<IDataStream> StreamDeleter;

    if (uri) {
        /* plugins get the first crack at the uri */
        for (const auto factory : DataStreamFactory::Instance()->dataStreamFactories) {
            if (factory->CanRead(uri)) {
                IDataStream* dataStream = factory->Open(uri, flags);

                if (dataStream) {
                    return dataStream;
                }
            }
        }

        /* no plugins accepted it? try to open as a local file */
        IDataStream* regularFile = new LocalFileStream();
        if (regularFile->Open(uri, flags)) {
            return regularFile;
        }
        else {
            regularFile->Release();
        }
    }

    return nullptr;
}

DataStreamPtr DataStreamFactory::OpenSharedDataStream(const char *uri, OpenFlags flags) {
    auto stream = OpenDataStream(uri, flags);
    return stream ? DataStreamPtr(stream, StreamDeleter()) : DataStreamPtr();
}
