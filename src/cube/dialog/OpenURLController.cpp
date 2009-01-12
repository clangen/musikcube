//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2009, Daniel Önnerby
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

#include <core/PlaybackQueue.h>
#include <core/TrackFactory.h>

#include <cube/dialog/OpenURLController.hpp>

#include <win32cpp/Window.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/EditView.hpp>
#include <win32cpp/RedrawLock.hpp>
//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

OpenURLController::OpenURLController(win32cpp::TopLevelWindow &mainWindow)
:mainWindow(mainWindow)
{
    this->view  = new OpenURLView();
    this->mainWindow.AddChild(this->view);
    this->mainWindow.Resized.connect(this,&OpenURLController::OnResize);
    
    this->view->Handle()
        ? this->OnViewCreated(this->view)
        : this->view->Created.connect(this, &OpenURLController::OnViewCreated);
    
}

OpenURLController::~OpenURLController(){
}

void OpenURLController::OnViewCreated(Window* window)
{
    this->view->cancelButton->Pressed.connect(this,&OpenURLController::OnCancel);
    this->view->okButton->Pressed.connect(this,&OpenURLController::OnOK);
}

void OpenURLController::OnCancel(win32cpp::Button* button){
    this->mainWindow.Close();
}

void OpenURLController::OnOK(win32cpp::Button* button){
    (*musik::core::PlaybackQueue::Instance().NowPlayingTracklist()) += musik::core::TrackFactory::CreateTrack( this->view->url->Caption().c_str() );
	this->mainWindow.Close();
}

void OpenURLController::OnResize(win32cpp::Window* window, win32cpp::Size size){
    win32cpp::RedrawLock redrawLock(this->view);
    this->view->Resize(this->mainWindow.ClientSize());
}
