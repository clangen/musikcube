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

#include <pch.hpp>
#include <cube/SettingsController.hpp>
#include <core/LibraryFactory.h>
#include <core/Indexer.h>

#include <win32cpp/FolderBrowseDialog.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    SettingsController::SettingsController(SettingsView& settingsView)
: settingsView(settingsView)
, libraryStatusTimer(300)
{
    this->settingsView.Created.connect(
        this, &SettingsController::OnViewCreated);
    
    this->settingsView.Resized.connect(
        this, &SettingsController::OnViewResized);
}

void        SettingsController::OnViewCreated(Window* window)
{

    this->settingsView.addPathButton->Pressed.connect(this,&SettingsController::OnAddPath);
    this->settingsView.removePathButton->Pressed.connect(this,&SettingsController::OnRemovePath);

    this->libraryStatusTimer.ConnectToWindow(&this->settingsView);
    this->libraryStatusTimer.OnTimout.connect(this,&SettingsController::OnLibraryStatus);
    this->libraryStatusTimer.Start();

    this->syncPathController.reset(new musik::cube::settings::SyncPathController(*this->settingsView.pathList,this));
}

void        SettingsController::OnViewResized(Window* window, Size size)
{
}

void SettingsController::OnAddPath(Button* button){
    win32cpp::FolderBrowseDialog addPath;

    if(addPath.Show(win32cpp::Application::Instance().MainWindow())==win32cpp::FolderBrowseDialog::ResultOK){
        musik::core::Indexer *indexer = musik::core::LibraryFactory::GetCurrentLibrary()->Indexer();
        if(indexer){
            indexer->AddPath(addPath.Directory());
        }
    }
}

void SettingsController::OnRemovePath(Button* button){
    this->syncPathController->RemoveSelectedPaths();
}

void SettingsController::OnLibraryStatus(){
    this->settingsView.libraryStatus->SetCaption( musik::core::LibraryFactory::GetCurrentLibrary()->GetInfo() );
}


