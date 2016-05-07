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

using namespace musik::core;

PluginFactory PluginFactory::sInstance;

PluginFactory::PluginFactory() {
    musik::debug::info(TAG, "loading plugins");
    this->LoadPlugins();
}

PluginFactory::~PluginFactory(void){
    for (size_t i=0; i<this->loadedPlugins.size(); ++i)
    {
        this->loadedPlugins[i]->Destroy();
    }

    // Unload dlls
    for(std::vector<void*>::iterator oDLL=this->loadedDLLs.begin();oDLL!=this->loadedDLLs.end();){
        #ifdef WIN32
//            FreeLibrary( (HMODULE)(*oDLL) );
	#else
	      dlclose(*oDLL);
        #endif
        oDLL    = this->loadedDLLs.erase(oDLL);
    }
}

void PluginFactory::LoadPlugins(){
    boost::mutex::scoped_lock lock(this->mutex);
    std::string sPluginDir(GetPluginDirectory());

    // Open plugin directory
    boost::filesystem::path oDir(sPluginDir);

    try{
        boost::filesystem::directory_iterator oEndFile;
        for(boost::filesystem::directory_iterator oFile(oDir);oFile!=oEndFile;++oFile){
            if(boost::filesystem::is_regular(oFile->status())){
                // This is a file
                std::string sFile(oFile->path().string());

                #ifdef WIN32
                    if(sFile.substr(sFile.size()-4)==".dll"){    // And a DLL

                        std::wstring wpath = u8to16(sFile);
                        HMODULE oDLL = LoadLibrary(wpath.c_str());
                        if(oDLL!=NULL){
                            CallGetPlugin getPluginCall = (CallGetPlugin)GetProcAddress(oDLL,"GetPlugin");
                            if(getPluginCall){
                                this->loadedPlugins.push_back(getPluginCall());
                                this->loadedDLLs.push_back(oDLL);
                            }else{
                                FreeLibrary( oDLL );
                            }
/*
                            // Check for tagreader
                            CallGetTagReader getTagReader = (CallGetTagReader)GetProcAddress(oDLL,"GetTagReader");
                            if(getTagReader){
                                this->aTagReaders.push_back(getTagReader());
                            }*/
                        }


                    }
		#else	//GNU or other
                    if(sFile.substr(sFile.size()-3)==".so"){    // And a shared lib
		        void* oDLL = dlopen(sFile.c_str(), RTLD_NOW);
			char* err;
			if ((err = dlerror()) != NULL) {
			    std::cerr << "Couldn't open shared library " << sFile << std::endl;
			    std::cerr << "Error: " << err << std::endl;
			}
			else	{
				IPlugin* getPluginCall = (IPlugin*)dlsym(oDLL,"GetPlugin");
                if(getPluginCall){
                	this->loadedPlugins.push_back(getPluginCall);
			        this->loadedDLLs.push_back(oDLL);
#ifdef _DEBUG
			        std::cout << "Shared library " << sFile.c_str() << " loaded and opened." << std::endl;
#endif
			    }
			    else	{
			        dlclose(oDLL);
			    }
			}
		    }
                #endif //WIN32
            }
        }
    }
    catch(...){
    }
}


