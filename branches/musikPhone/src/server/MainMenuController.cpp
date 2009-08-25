//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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
#include <server/MainMenuController.hpp>
#include <server/dialog/HelpAboutController.hpp>

#include <win32cpp/Application.hpp>
#include <win32cpp/TopLevelWindow.hpp>
#include <boost/format.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::server;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MainMenuController::MainMenuController(TopLevelWindow& mainWindow)
: mainWindow(mainWindow)
{
    this->mainWindow.Created.connect(
        this, &MainMenuController::OnMainWindowCreated);
}

MainMenuController::~MainMenuController(){
}

void        MainMenuController::OnMainWindowCreated(Window* window)
{
    this->mainWindow.SetMenu(this->CreateMenu());
}

void        MainMenuController::ConnectMenuHandlers()
{
    this->fileExit->Activated.connect(this, &MainMenuController::OnFileExit);
    this->helpAbout->Activated.connect(this, &MainMenuController::OnHelpAbout);
	this->fileSettings->Activated.connect(this, &MainMenuController::OnFileSettings);
}

void        MainMenuController::OnFileExit(MenuItemRef menuItem)
{
    Application::Instance().Terminate();
}

void        MainMenuController::OnFileSettings(MenuItemRef menuItem){
}


void        MainMenuController::OnHelpAbout(MenuItemRef menuItem)
{
    win32cpp::TopLevelWindow aboutDialog(_T("About mC2"));
    aboutDialog.SetMinimumSize(Size(400,300));

    dialog::HelpAboutController helpAbout(aboutDialog);

    aboutDialog.ShowModal(&this->mainWindow);

    return;
}


MenuRef     MainMenuController::CreateMenu()
{
    // main menu
    this->mainMenu  = Menu::Create();
    MenuItemCollection& mainItems = this->mainMenu->Items();
    //
    this->file = mainItems.Append(MenuItem::Create(UTF("&File")));
    this->help = mainItems.Append(MenuItem::Create(UTF("&Help")));

        // file menu
        this->fileMenu  = Menu::Create();
        MenuItemCollection& fileItems = this->fileMenu->Items();
        //
        this->file->SetSubMenu(this->fileMenu);

        this->fileSettings			= fileItems.Append(MenuItem::Create(UTF("&Settings")));
        this->fileExit              = fileItems.Append(MenuItem::Create(UTF("E&xit")));

        // help menu
        this->helpMenu  = Menu::Create();
        MenuItemCollection& helpItems = this->helpMenu->Items();
        //
        this->help->SetSubMenu(this->helpMenu);
        this->helpAbout = helpItems.Append(MenuItem::Create(UTF("&About")));

    this->ConnectMenuHandlers();

    return this->mainMenu;
}
