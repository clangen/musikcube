//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 Team
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
#include <core/Crypt.h>
#include <core/Common.h>
#include <cube/dialog/HelpAboutController.hpp>
#include <cube/dialog/HelpAboutView.hpp>

#include <win32cpp/Window.hpp>
#include <win32cpp/Button.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube::dialog;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

HelpAboutController::HelpAboutController(win32cpp::TopLevelWindow &mainWindow)
:mainWindow(mainWindow)
,view(NULL)
{
    this->view  = new HelpAboutView;
    this->mainWindow.AddChild(this->view);
    
    this->view->Created.connect(this, &HelpAboutController::OnViewCreated);

    // Start drawing thread
    this->view->StartDrawingThread();
}

HelpAboutController::~HelpAboutController() 
{
}

void HelpAboutController::OnViewCreated(Window* window) 
{
    this->view->okButton->Pressed.connect(this, &HelpAboutController::OnOK);
}

void HelpAboutController::OnOK(win32cpp::Button* button)
{
    this->mainWindow.Close();
}
