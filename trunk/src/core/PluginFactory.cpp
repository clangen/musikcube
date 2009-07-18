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
#include <core/PluginFactory.h>
#include <core/config_filesystem.h>
#include <core/Common.h>


using namespace musik::core;

PluginFactory PluginFactory::sInstance;

PluginFactory::PluginFactory(void){
    this->LoadPlugins();
}

PluginFactory::~PluginFactory(void){
    for (size_t i=0; i<this->loadedPlugins.size(); ++i)
    {
        this->loadedPlugins[i]->Destroy();
    }
    //this->loadedPlugins.clear();		//This causes a segmentation fault on linux due to loadedPlugins being already empty. Jooles
    //Also somewhere here is another seg fault that only seems to occur when the program is run from within its directory
    //eg. cd'ing to musikCube/bin and running square then quitting segfaults here. cd'ing to musikCube and running
    // bin/square then quitting works fine.. Any ideas anyone? Jooles
#ifdef _DEBUG
    std::cerr << "Unloading DLLs" << std::endl;
#endif
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
    utfstring sPluginDir(GetPluginDirectory());

    // Open plugin directory
    boost::filesystem::utfpath oDir(sPluginDir);

    try{
        boost::filesystem::utfdirectory_iterator oEndFile;
        for(boost::filesystem::utfdirectory_iterator oFile(oDir);oFile!=oEndFile;++oFile){
            if(boost::filesystem::is_regular(oFile->status())){
                // This is a file
                utfstring sFile(oFile->path().string());

                #ifdef WIN32
                    if(sFile.substr(sFile.size()-4)==UTF(".dll")){    // And a DLL

                        HMODULE oDLL = LoadLibrary(sFile.c_str());
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
                    if(sFile.substr(sFile.size()-3)==UTF(".so")){    // And a shared lib
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


