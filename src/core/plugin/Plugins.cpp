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
#include "Plugins.h"
#include "PluginFactory.h"

#include <core/support/Common.h>
#include <core/support/Preferences.h>

#include <core/io/DataStreamFactory.h>
#include <core/audio/Streams.h>

#include <core/library/LocalSimpleDataProvider.h>

#include <core/sdk/IIndexerNotifier.h>
#include <core/sdk/IEnvironment.h>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::core::io;
using namespace musik::core::sdk;

static class Environment : public IEnvironment {
    public:
        virtual size_t GetPath(PathType type, char* dst, int size) override {
            std::string path;
            switch (type) {
                case PathUserHome: path = GetHomeDirectory(); break;
                case PathData: path = GetDataDirectory(); break;
                case PathApplication: path = GetApplicationDirectory(); break;
                case PathPlugins: path = GetPluginDirectory(); break;
            }
            return CopyString(path, dst, size);
        }

        virtual IDataStream* GetDataStream(const char* uri) override {
            return DataStreamFactory::OpenDataStream(uri);
        }

        virtual IDecoder* GetDecoder(IDataStream* stream) override {
            return streams::GetDecoderForDataStream(stream);
        }
} environment;

typedef void(*SetEnvironment)(IEnvironment*);

typedef void(*SetSimpleDataProvider)(ISimpleDataProvider*);
LocalSimpleDataProvider* dataProvider = nullptr;

typedef void(*SetIndexerNotifier)(IIndexerNotifier*);

namespace musik { namespace core { namespace plugin {

    void InstallDependencies(ILibraryPtr library) {
        /* preferences */
        Preferences::LoadPluginPreferences();

        /* data providers */
        delete dataProvider;
        dataProvider = new LocalSimpleDataProvider(library);

        PluginFactory::Instance().QueryFunction<SetSimpleDataProvider>(
            "SetSimpleDataProvider",
            [](musik::core::sdk::IPlugin* plugin, SetSimpleDataProvider func) {
                func(dataProvider);
            });

        /* indexer */
        IIndexerNotifier* indexerNotifier =
            dynamic_cast<IIndexerNotifier*>(library->Indexer());

        PluginFactory::Instance().QueryFunction<SetIndexerNotifier>(
            "SetIndexerNotifier",
            [indexerNotifier](musik::core::sdk::IPlugin* plugin, SetIndexerNotifier func) {
                func(indexerNotifier);
            });

        /* environment */
        PluginFactory::Instance().QueryFunction<SetEnvironment>(
            "SetEnvironment",
            [](musik::core::sdk::IPlugin* plugin, SetEnvironment func) {
                func(&environment);
            });
    }

    void UninstallDependencies() {
        /* preferences */
        Preferences::SavePluginPreferences();

        /* data providers */
        PluginFactory::Instance().QueryFunction<SetSimpleDataProvider>(
            "SetSimpleDataProvider",
            [](musik::core::sdk::IPlugin* plugin, SetSimpleDataProvider func) {
                func(nullptr);
            });

        delete dataProvider;
        dataProvider = nullptr;

        /* indexer */
        PluginFactory::Instance().QueryFunction<SetIndexerNotifier>(
            "SetIndexerNotifier",
                [](musik::core::sdk::IPlugin* plugin, SetIndexerNotifier func) {
                func(nullptr);
            });

        /* environment */
        PluginFactory::Instance().QueryFunction<SetEnvironment>(
            "SetEnvironment",
            [](musik::core::sdk::IPlugin* plugin, SetEnvironment func) {
                func(nullptr);
            });
    }

} } }
