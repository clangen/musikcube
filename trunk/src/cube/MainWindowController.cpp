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
#include <cube/MainWindowController.hpp>
#include <cube/TransportView.hpp>
#include <cube/LibraryWindowController.hpp>
#include <cube/LibraryWindowView.hpp>
#include <cube/TransportController.hpp>

#include <core/Preferences.h>

#include <cube/resources/resource.h>

#include <win32cpp/Types.hpp>    // uichar, uistring
#include <win32cpp/TopLevelWindow.hpp>
#include <win32cpp/Splitter.hpp>
#include <win32cpp/TabView.hpp>
#include <win32cpp/RedrawLock.hpp>
#include <win32cpp/SysTray.hpp> 

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MainWindowController::MainWindowController(TopLevelWindow& mainWindow)
: mainWindow(mainWindow)
, transportController(NULL)
, menuController(mainWindow)
, libraryController(NULL)
{

    this->mainWindow.Created.connect(
        this, &MainWindowController::OnMainWindowCreated);

}

MainWindowController::~MainWindowController()
{
    delete this->libraryController;
    delete this->transportController;
}

void        MainWindowController::OnFileExit(MenuItemRef menuItem)
{
    Application::Instance().Terminate();
}

void        MainWindowController::OnMainWindowCreated(Window* window)
{
    
    // Start by setting the icon
    HICON icon16 = (HICON)LoadImage(Application::Instance(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    HICON icon32 = (HICON)LoadImage(Application::Instance(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    SendMessage( window->Handle(), WM_SETICON, WPARAM( ICON_SMALL ), LPARAM( icon16 ) );
    SendMessage( window->Handle(), WM_SETICON, WPARAM( ICON_BIG ), LPARAM( icon32 ) );


    // Init Tray Icon
    MenuRef myMenu = Menu::CreatePopup();

    // Create Tray Menu
    myMenu->Items().Append(MenuItem::Create(_T("Test 1")));
    myMenu->Items().Append(MenuItem::Create(_T("Test 2")));
    MenuItemRef trayExit = myMenu->Items().Append(MenuItem::Create(_T("E&xit")));

    // Bind Exit to handler
    trayExit->Activated.connect(this, &MainWindowController::OnFileExit);

    UINT uidTrayIcon = Application::Instance().SysTrayManager()->AddIcon(Application::Instance().MainWindow(), icon16);
    Application::Instance().SysTrayManager()->SetTooltip(uidTrayIcon, _T("musikCube"));
    Application::Instance().SysTrayManager()->SetPopupMenu(uidTrayIcon, myMenu);
    Application::Instance().SysTrayManager()->ShowBalloon(uidTrayIcon, _T("musikCube 2"), _T("Welcome to musikCube!"), 2);
    Application::Instance().SysTrayManager()->EnableMinimizeToTray(uidTrayIcon);
        

    static const int TransportViewHeight = 54;

    {
        musik::core::Preferences windowPrefs("MainWindow");
        this->mainWindow.MoveTo(windowPrefs.GetInt("x",200), windowPrefs.GetInt("y",200));
        this->mainWindow.Resize(windowPrefs.GetInt("width",700), windowPrefs.GetInt("height",480));
        this->mainWindow.SetMinimumSize(Size(320, 240));
    }

    Size clientSize = this->mainWindow.ClientSize();

    // create transport view/controller
    TransportView* transportView = new TransportView();
    transportView->SetPadding(FramePadding(2, 4, 0, 0));
    this->transportController = new TransportController(*transportView);

    // create library view/controller
    LibraryWindowView* libraryView = new LibraryWindowView();
    this->libraryController = new LibraryWindowController(*libraryView);


    // the main splitter
    Splitter* transportSplitter = this->mainWindow.AddChild(
        new Splitter(SplitRow, libraryView, transportView));

    // set initial sizes
    transportSplitter->Resize(clientSize);
    transportSplitter->SetAnchor(AnchorBottom);
    transportSplitter->SetAnchorSize(TransportViewHeight);
    transportSplitter->SetSizable(false);

    this->clientView = transportSplitter;
    this->mainWindow.Resized.connect(this, &MainWindowController::OnResize);
    this->mainWindow.Destroyed.connect(this, &MainWindowController::OnDestroyed);
}

void MainWindowController::OnResize(Window* window, Size size)
{
    RedrawLock redrawLock(this->clientView);
    this->clientView->Resize(this->mainWindow.ClientSize());
}

void MainWindowController::OnDestroyed(Window* window)
{
    Point location( window->Location() );
    Size size( window->WindowSize() );
    musik::core::Preferences windowPrefs("MainWindow");
    windowPrefs.SetInt("x",location.x);
    windowPrefs.SetInt("y",location.y);
    windowPrefs.SetInt("width",size.width);
    windowPrefs.SetInt("height",size.height);
}
