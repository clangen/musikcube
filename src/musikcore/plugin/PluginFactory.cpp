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

#include <musikcore/sdk/constants.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Common.h>
#include <musikcore/config.h>
#include <musikcore/debug.h>
#include <musikcore/utfutil.h>
#include <iostream>
#include <filesystem>

static const std::string TAG = "PluginFactory";
static std::mutex instanceMutex;

namespace fs = std::filesystem;

using namespace musik::core;

#ifdef WIN32
typedef musik::core::sdk::IPlugin* STDCALL(CallGetPlugin);
static void closeNativeHandle(void* dll) { FreeLibrary((HMODULE)dll); }
#else
typedef musik::core::sdk::IPlugin* (*CallGetPlugin)();
static void closeNativeHandle(void* dll) { dlclose(dll); }
#endif

PluginFactory& PluginFactory::Instance() {
    std::unique_lock<std::mutex> lock(instanceMutex);

    static PluginFactory* instance = NULL;

    if (instance == NULL) {
        instance = new PluginFactory();
    }

    return *instance;
}

PluginFactory::PluginFactory() {
    this->prefs = Preferences::ForComponent(prefs::components::Plugins);
    musik::debug::info(TAG, "loading plugins");
    this->LoadPlugins();
}

PluginFactory::~PluginFactory() {
    for (std::shared_ptr<Descriptor> plugin : this->plugins) {
        plugin->plugin->Release();
        closeNativeHandle(plugin->nativeHandle);
    }
    plugins.clear();
}

void PluginFactory::LoadPlugins() {
#ifdef WIN32
    {
        std::wstring wpath = u8to16(GetPluginDirectory());
        SetDllDirectory(wpath.c_str());
    }
#endif

    std::string pluginDir(GetPluginDirectory());
    fs::path dir(fs::u8path(pluginDir));

    try {
        fs::directory_iterator end;
        for (fs::directory_iterator file(dir); file != end; file++) {
            if (fs::is_regular_file(file->status())){
                std::string filename = fs::canonical(fs::path(file->path()).make_preferred()).u8string();
                std::shared_ptr<Descriptor> descriptor(new Descriptor());
                descriptor->filename = filename;
                descriptor->key = fs::path(fs::u8path(filename)).filename().u8string();
#ifdef WIN32
                /* if the file ends with ".dll", we'll try to load it*/
                if (filename.substr(filename.size() - 4) == ".dll") {
                    HMODULE dll = LoadLibrary(u8to16(filename).c_str());
                    if (dll != NULL) {
                        /* every plugin has a "GetPlugin" method. */
                        CallGetPlugin getPluginCall = (CallGetPlugin) GetProcAddress(dll, "GetPlugin");

                        if (getPluginCall) {
                            /* exists? check the version, and add it! */
                            auto plugin = getPluginCall();
                            if (plugin->SdkVersion() == musik::core::sdk::SdkVersion) {
                                descriptor->plugin = plugin;
                                descriptor->nativeHandle = dll;
                                this->plugins.push_back(descriptor);
                                musik::debug::info(TAG, "loaded: " + filename);
                            }
                            else {
                                musik::debug::error(TAG, "plugin with invalid version: " + filename);
                                FreeLibrary(dll);
                            }
                        }
                        else {
                            /* otherwise, free and move on */
                            musik::debug::error(TAG, "plugin entry point not found: " + filename);
                            FreeLibrary(dll);
                        }
                    }
                }
#else
    #ifdef __APPLE__
                if (filename.substr(filename.size() - 6) == ".dylib") {
                    int openFlags = RTLD_LOCAL;
    #else
                if (filename.substr(filename.size() - 3) == ".so") {
                    int openFlags = RTLD_NOW;
    #endif
                    void* dll = NULL;

                    try {
                        dll = dlopen(filename.c_str(), openFlags);
                    }
                    catch (...) {
                        musik::debug::error(TAG, "exception while loading plugin " + filename);
                        continue;
                    }

                    if (!dll) {
                        char *err = dlerror();

                        musik::debug::error(TAG, u8fmt("exception while loading plugin %s: %s", filename.c_str(), err).c_str());
                    }
                    else {
                        CallGetPlugin getPluginCall;
                        *(void **)(&getPluginCall) = dlsym(dll, "GetPlugin");

                        if (getPluginCall) {
                            auto plugin = getPluginCall();
                            if (plugin->SdkVersion() == musik::core::sdk::SdkVersion) {
                                musik::debug::info(TAG, "loaded: " + filename);
                                descriptor->plugin = getPluginCall();
                                descriptor->nativeHandle = dll;
                                this->plugins.push_back(descriptor);
                            }
                            else {
                                dlclose(dll);
                            }
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
