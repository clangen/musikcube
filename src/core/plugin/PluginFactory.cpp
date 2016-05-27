//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Daniel �nnerby
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

#include <core/plugin/PluginFactory.h>
#include <core/config.h>
#include <core/support/Common.h>
#include <core/debug.h>

static const std::string TAG = "PluginFactory";
static boost::mutex instanceMutex;

using namespace musik::core;

PluginFactory& PluginFactory:: Instance() {
    boost::mutex::scoped_lock lock(instanceMutex);

    static PluginFactory* instance = NULL;

    if (instance == NULL) {
        instance = new PluginFactory();
    }

    return *instance;
}

PluginFactory::PluginFactory() {
    musik::debug::info(TAG, "loading plugins");
    this->LoadPlugins();
}

PluginFactory::~PluginFactory(void){
    for (size_t i = 0; i < this->loadedPlugins.size(); i++) {
        this->loadedPlugins[i]->Destroy();
    }

    std::vector<void*>::iterator dll = this->loadedDlls.begin();
    for( ; dll != this->loadedDlls.end(); dll++) {
        #ifdef WIN32
            FreeLibrary((HMODULE) (*dll));
        #else
            dlclose(*dll);
        #endif
    }

    loadedDlls.clear();
}

void PluginFactory::LoadPlugins(){
    boost::mutex::scoped_lock lock(this->mutex);

    std::string pluginDir(GetPluginDirectory());
    boost::filesystem::path dir(pluginDir);

    try {
        boost::filesystem::directory_iterator end;
        for (boost::filesystem::directory_iterator file(dir); file != end; file++) {
            if (boost::filesystem::is_regular(file->status())){
                std::string filename(file->path().string());
#ifdef WIN32
                /* if the file ends with ".dll", we'll try to load it*/

                if (filename.substr(filename.size() - 4) == ".dll") {

                    HMODULE dll = LoadLibrary(u8to16(filename).c_str());
                    if (dll != NULL) {
                        /* every plugin has a "GetPlugin" method. */
                        CallGetPlugin getPluginCall = (CallGetPlugin) GetProcAddress(dll, "GetPlugin");

                        if (getPluginCall) {
                            /* exists? add it! */
                            this->loadedPlugins.push_back(getPluginCall());
                            this->loadedDlls.push_back(dll);
                        }
                        else {
                            /* otherwise, free nad move on */
                            FreeLibrary(dll);
                        }
                    }
                }
#elif __APPLE__
                if (filename.substr(filename.size() - 6) == ".dylib") {
                    void* dll = NULL;
                    char* err;

                    try {
                        dll = dlopen(filename.c_str(), RTLD_LOCAL);
                    }
                    catch (...) {
                        musik::debug::err(TAG, "exception while loading plugin " + filename);
                        continue;
                    }

                    if (err != NULL || (err = dlerror()) != NULL) {
                        musik::debug::err(
                            TAG,
                            "could not load shared library " + filename +
                            " error: " + std::string(err));
                    }
                    else {
                        CallGetPlugin getPluginCall;
                        *(void **)(&getPluginCall) = dlsym(dll, "GetPlugin");

                        if (getPluginCall) {
                            musik::debug::info(TAG, "loaded: " + filename);
                            this->loadedPlugins.push_back(getPluginCall());
                            this->loadedDlls.push_back(dll);
                        }
                        else {
                            dlclose(dll);
                        }
                    }
                }
#endif
            }
        }
    }
    catch(...) {
    }
}
