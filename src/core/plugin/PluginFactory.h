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

#pragma once

#include <core/config.h>
#include <core/sdk/IPlugin.h>

#include <vector>
#include <iostream>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#ifndef WIN32
    #include <dlfcn.h>
#endif

namespace musik { namespace core {

    class PluginFactory {
        public:
            static PluginFactory& Instance();

        private:

            PluginFactory();
            ~PluginFactory();
            void LoadPlugins();

        private:
#ifdef WIN32
            typedef musik::core::IPlugin* STDCALL(CallGetPlugin);
#else
            typedef musik::core::IPlugin* (*CallGetPlugin)();
#endif

            typedef std::vector<IPlugin*> PluginList;
            typedef std::vector<void*> HandleList;

            PluginList loadedPlugins;
            HandleList loadedDlls;
            boost::mutex mutex;

        public:

            template <typename T>
            class  DestroyDeleter {
            public: void operator()(T* t) {
                    t->Destroy();
                }
            };

            template <typename T>
            class  NullDeleter {
            public: void operator()(T* t) {
                }
            };

            template <class T, class D> std::vector<std::shared_ptr<T> > QueryInterface(const char* functionName) {
                boost::mutex::scoped_lock lock(this->mutex);

                typedef T* STDCALL(PluginInterfaceCall);

                std::vector<std::shared_ptr<T> > plugins;
                HandleList& allDlls = PluginFactory::Instance().loadedDlls;

                typedef HandleList::iterator Iterator;
                Iterator currentDll = allDlls.begin();
                while (currentDll != allDlls.end()) {
                    PluginInterfaceCall funcPtr =
#ifdef WIN32
                        (PluginInterfaceCall) GetProcAddress((HMODULE)(*currentDll), functionName);
#else
                        (PluginInterfaceCall) dlsym(*currentDll, functionName);
#endif
                    if (funcPtr) {
                        T* result = funcPtr();

                        if (result) {
                            plugins.push_back(std::shared_ptr<T>(result, D()));
                        }
                    }

                    currentDll++;
                }

                return plugins;
            }
    };
} }
