//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 team
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
#include <server/MainWindowController.hpp>

#include <server/resources/resource.h>
#include <server/SyncpathController.hpp>
#include <server/SyncpathView.hpp>
#include <server/users/UsersController.hpp>
#include <server/users/UsersView.hpp>

#include <core/Preferences.h>

#include <win32cpp/Types.hpp>    // uichar, uistring
#include <win32cpp/TopLevelWindow.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/TabView.hpp>
#include <win32cpp/RedrawLock.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/Frame.hpp>
#include <win32cpp/TrayIconManager.hpp> 

//////////////////////////////////////////////////////////////////////////////

using namespace musik::server;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MainWindowController::MainWindowController(TopLevelWindow& mainWindow,musik::core::ServerPtr server)
 :mainWindow(mainWindow)
 ,server(server)
 ,mainFrame(NULL)
 ,syncpathController(NULL)
 ,usersController(NULL)
{
    this->mainWindow.Handle()
        ? this->OnMainWindowCreated(&this->mainWindow)
        : this->mainWindow.Created.connect(this, &MainWindowController::OnMainWindowCreated);
}

MainWindowController::~MainWindowController()
{
    delete this->syncpathController;
    delete this->usersController;
}

void        MainWindowController::OnMainWindowCreated(Window* window)
{
    RedrawLock redrawLock(window);

    {
        // Set window size and position
        musik::core::Preferences windowPrefs("ServerSettingsWindow");
        this->mainWindow.MoveTo(windowPrefs.GetInt("x",200), windowPrefs.GetInt("y",200));
        this->mainWindow.Resize(windowPrefs.GetInt("width",500), windowPrefs.GetInt("height",400));
        this->mainWindow.SetMinimumSize(Size(320, 240));
    }


    FontRef boldFont(Font::Create());
    boldFont->SetBold(true);


    // Create the layout
    this->mainFrame              = new win32cpp::Frame(NULL,win32cpp::FramePadding(4));

    // Second a TabView for the settings
    win32cpp::TabView *tabs         = this->mainFrame->AddChild( new win32cpp::TabView() );

    // Syncpath tab
    SyncpathView *synpathView   = tabs->AddTab(uistring(_T("Sync paths")), new SyncpathView());
    this->syncpathController    = new SyncpathController(*synpathView,&this->server->indexer);

    // Users tab
    users::UsersView *usersView = tabs->AddTab(uistring(_T("Users")), new users::UsersView());
    this->usersController       = new users::UsersController(*usersView,this->server.get());

    // Settings tab
    Frame *settingsView   = tabs->AddTab(uistring(_T("Settings")), new Frame());

    this->mainWindow.AddChild(mainFrame);

    // Connect size and position signals
    this->mainWindow.Resized.connect(this, &MainWindowController::OnResize);
    this->mainWindow.Destroyed.connect(this, &MainWindowController::OnDestroyed);


}

void MainWindowController::OnResize(Window* window, Size size)
{
    RedrawLock redrawLock(window);
    this->mainFrame->Resize(this->mainWindow.ClientSize());
}

void MainWindowController::OnDestroyed(Window* window)
{
    Point location( window->Location() );
    Size size( window->WindowSize() );
    musik::core::Preferences windowPrefs("ServerSettingsWindow");
    windowPrefs.SetInt("x",location.x);
    windowPrefs.SetInt("y",location.y);
    windowPrefs.SetInt("width",size.width);
    windowPrefs.SetInt("height",size.height);
}

