//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 Team
//
// Sources and Binaries of: mC2, win32cpp
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
#include <core/LibraryFactory.h>
#include <core/Preferences.h>
#include <cube/dialog/AddLibraryController.hpp>
#include <cube/dialog/AddLibraryView.hpp>

#include <win32cpp/Window.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/EditView.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

AddLibraryController::AddLibraryController(win32cpp::TopLevelWindow &mainWindow,int type)
:mainWindow(mainWindow)
,view(NULL)
,libraryType(type)
{
    this->view  = new AddLibraryView(type);
    this->mainWindow.AddChild(this->view);
    
    this->view->Handle()
        ? this->OnViewCreated(this->view)
        : this->view->Created.connect(this, &AddLibraryController::OnViewCreated);
    
}

AddLibraryController::~AddLibraryController(){
}

void AddLibraryController::OnViewCreated(Window* window)
{
    this->view->cancelButton->Pressed.connect(this,&AddLibraryController::OnCancel);
    this->view->okButton->Pressed.connect(this,&AddLibraryController::OnOK);
}

void AddLibraryController::OnCancel(win32cpp::Button* button){
    this->mainWindow.Close();
}

void AddLibraryController::OnOK(win32cpp::Button* button){
    if(musik::core::LibraryPtr library=musik::core::LibraryFactory::Instance().CreateLibrary(this->view->name->Caption(),this->libraryType,false)){
        if(this->libraryType==musik::core::LibraryFactory::Remote){
            musik::core::Preferences prefs("Connection",this->view->name->Caption().c_str() );
            prefs.SetString("address",this->view->remoteHost->Caption().c_str());
            prefs.SetString("port",this->view->remotePort->Caption().c_str());
        }

        library->Startup();

        this->mainWindow.Close();
    }else{
        // TODO: Show some error that it didn't work
        this->mainWindow.Close();
    }
}
