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

#include <core/io/DataStreamFactory.h>
#include <core/config.h>
#include <core/plugin/PluginFactory.h>
#include <core/io/LocalFileStream.h>

using namespace musik::core::io;
using namespace musik::core::sdk;

DataStreamFactory::DataStreamFactory() {
    typedef IDataStreamFactory PluginType;
    typedef musik::core::PluginFactory::DestroyDeleter<PluginType> Deleter;

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

DataStreamFactory::DataStreamPtr DataStreamFactory::OpenUri(const char *uri) {
    typedef musik::core::PluginFactory::DestroyDeleter<IDataStream> StreamDeleter;

    if (uri) {
        DataStreamFactoryVector::iterator it =
            DataStreamFactory::Instance()->dataStreamFactories.begin();

        /* plugins get the first crack at the uri */
        for( ; it != DataStreamFactory::Instance()->dataStreamFactories.end(); it++) {
            if ((*it)->CanRead(uri)) {
                IDataStream* dataStream = (*it)->Open(uri);

                if (dataStream) {
                    return DataStreamPtr(dataStream, StreamDeleter());
                }
            }
        }

        /* no plugins accepted it? try to open as a local file */
        DataStreamPtr regularFile(new LocalFileStream(), StreamDeleter());

        if (regularFile->Open(uri)) {
            return regularFile;
        }
    }

    return DataStreamPtr();
}

bool DataStreamFactory::IsLocalFileStream(const char *uri) {
    typedef musik::core::PluginFactory::DestroyDeleter<IDataStream> StreamDeleter;

    if (uri) {
        /* see if a plugin can handle this. if it can, then it's not
        considered to be a local file stream */
        DataStreamFactoryVector::iterator it =
            DataStreamFactory::Instance()->dataStreamFactories.begin();

        for( ; it != DataStreamFactory::Instance()->dataStreamFactories.end(); ++it) {
            if ((*it)->CanRead(uri)) {
                return false;
            }
        }

        /* now test local filesystem */
        boost::filesystem::path filename(uri);
        try {
            return boost::filesystem::exists(filename);
        }
        catch(...) {
        }
    }

    return false;
}
