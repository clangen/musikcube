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
#include <cube/MainMenuController.hpp>
//#include <cube/dialog/AddLibraryController.hpp>
//#include <cube/dialog/AddLibraryView.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/TopLevelWindow.hpp>
#include <boost/format.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MainMenuController::MainMenuController(TopLevelWindow& mainWindow)
: mainWindow(mainWindow)
//,addLibraryController(NULL)
{
    this->mainWindow.Created.connect(
        this, &MainMenuController::OnMainWindowCreated);
}

MainMenuController::~MainMenuController(){
//    delete this->addLibraryController;
}

void        MainMenuController::OnMainWindowCreated(Window* window)
{
    this->mainWindow.SetMenu(this->CreateMenu());
}

void        MainMenuController::ConnectMenuHandlers()
{
    this->fileExit->Activated.connect(this, &MainMenuController::OnFileExit);
    this->helpAbout->Activated.connect(this, &MainMenuController::OnHelpAbout);
    this->fileAddLibrary->Activated.connect(this,&MainMenuController::OnAddLibrary);
}

void        MainMenuController::OnFileExit(MenuItemRef menuItem)
{
    Application::Instance().Terminate();
}

void        MainMenuController::OnAddLibrary(MenuItemRef menuItem)
{
/*    dialog::AddLibraryView* addLibraryView  = new dialog::AddLibraryView();
    this->addLibraryController  = new dialog::AddLibraryController(*addLibraryView);*/
}

void        MainMenuController::OnHelpAbout(MenuItemRef menuItem)
{
    // randomize the contribuitors' names
    std::vector<uistring> names;
    names.push_back(_T("  - avatar\n"));
    names.push_back(_T("  - bjorn\n"));
    names.push_back(_T("  - doep\n"));
    names.push_back(_T("  - naaina\n"));
    names.push_back(_T("  - Jooles\n"));

    std::random_shuffle(names.begin(), names.end());

    uistring randomNames;
    for (std::vector<uistring>::iterator it = names.begin(); it != names.end(); it++)
    {
        randomNames += *it;
    }

    uistring message =
        _T("mC2 are copyright (c) mC2 Team 2007-2008\n")
        _T("win32cpp are copyright (c) Casey Langen 2007-2008\n\n")
        _T("Credits:\n")
        _T("%1%\n")
        _T("mC2 wouldn't be possible without these file projects:\n")
        _T("  - tuniac (http://tuniac.sf.net)\n")
        _T("  - boost (http://www.boost.org)\n")
        _T("  - sqlite3 (http://www.sqlite.org)\n")
        _T("  - taglib (http://developer.kde.org/~wheeler/taglib)\n\n")
        _T("Version 2 developer milestone 1");

    message = (boost::wformat(message.c_str()) % randomNames).str();

    ::MessageBox(
        this->mainWindow.Handle(),
        message.c_str(),
        _T("mC2 - About"),
        MB_ICONINFORMATION | MB_OK);
}

MenuRef     MainMenuController::CreateMenu()
{
    // main menu
    this->mainMenu  = Menu::Create();
    MenuItemCollection& mainItems = this->mainMenu->Items();
    //
    this->file = mainItems.Append(MenuItem::Create(_T("&File")));
    this->view = mainItems.Append(MenuItem::Create(_T("&View")));
    this->audio = mainItems.Append(MenuItem::Create(_T("&Audio")));
    this->tags = mainItems.Append(MenuItem::Create(_T("&Tags")));
    this->help = mainItems.Append(MenuItem::Create(_T("&Help")));

        // file menu
        this->fileMenu  = Menu::Create();
        MenuItemCollection& fileItems = this->fileMenu->Items();
        //
        this->file->SetSubMenu(this->fileMenu);

        this->fileAddLibrary    = fileItems.Append(MenuItem::Create(_T("&Add Library")));
        this->fileExit          = fileItems.Append(MenuItem::Create(_T("E&xit")));

        // help menu
        this->helpMenu  = Menu::Create();
        MenuItemCollection& helpItems = this->helpMenu->Items();
        //
        this->help->SetSubMenu(this->helpMenu);
        this->helpAbout = helpItems.Append(MenuItem::Create(_T("&About")));

    this->ConnectMenuHandlers();

    return this->mainMenu;
}
