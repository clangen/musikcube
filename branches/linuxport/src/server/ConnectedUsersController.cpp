//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2007, mC2 team
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
#include <server/ConnectedUsersController.hpp>
#include <server/ConnectedUsersListController.hpp>

#include <server/MainWindowController.hpp> 

#include <server/resources/resource.h>
#include <core/Preferences.h>

#include <win32cpp/Types.hpp>    // uichar, uistring
#include <win32cpp/TopLevelWindow.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/ApplicationThread.hpp>
#include <win32cpp/ListView.hpp>
#include <win32cpp/RedrawLock.hpp>
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/Frame.hpp>
#include <win32cpp/TrayIconManager.hpp> 


//////////////////////////////////////////////////////////////////////////////

using namespace musik::server;
using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    ConnectedUsersController::ConnectedUsersController(TopLevelWindow& mainWindow,musik::core::ServerPtr server)
 :mainWindow(mainWindow)
 ,server(server)
 ,mainFrame(NULL)
 ,timer(200)
 ,statusLabel(NULL)
 ,listViewController(NULL)
{

    this->mainWindow.Created.connect(
        this, &ConnectedUsersController::OnMainWindowCreated);

}

ConnectedUsersController::~ConnectedUsersController()
{
    delete this->listViewController;
}

void ConnectedUsersController::OnMainWindowCreated(Window* window)
{
    
    // Start by setting the icon
    HICON icon16 = (HICON)LoadImage(Application::Instance(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    HICON icon32 = (HICON)LoadImage(Application::Instance(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    SendMessage( window->Handle(), WM_SETICON, WPARAM( ICON_SMALL ), LPARAM( icon16 ) );
    SendMessage( window->Handle(), WM_SETICON, WPARAM( ICON_BIG ), LPARAM( icon32 ) );


    // Init Tray Icon
    MenuRef myMenu = Menu::CreatePopup();

    // Create Tray Menu
    MenuItemRef settingsMenu    = myMenu->Items().Append(MenuItem::Create(_T("S&ettings")));
    MenuItemRef trayExit        = myMenu->Items().Append(MenuItem::Create(_T("E&xit")));

    // Bind Exit to handler
    trayExit->Activated.connect(this, &ConnectedUsersController::OnFileExit);
    settingsMenu->Activated.connect(this, &ConnectedUsersController::OnSettings);

    UINT uidTrayIcon = Application::Instance().SysTrayManager()->AddIcon(Application::Instance().MainWindow(), icon16);
    Application::Instance().SysTrayManager()->SetTooltip(uidTrayIcon, _T("musikServer"));
    Application::Instance().SysTrayManager()->SetPopupMenu(uidTrayIcon, myMenu);
    Application::Instance().SysTrayManager()->EnableMinimizeToTray(uidTrayIcon);

    {
        // Set window size and position
        musik::core::Preferences windowPrefs("ServerUsersWindow");
        this->mainWindow.MoveTo(windowPrefs.GetInt("x",200), windowPrefs.GetInt("y",200));
        this->mainWindow.Resize(windowPrefs.GetInt("width",240), windowPrefs.GetInt("height",320));
        this->mainWindow.SetMinimumSize(Size(240, 320));
    }


    FontRef boldFont(Font::Create());
    boldFont->SetBold(true);


    // Create the layout
    this->mainFrame              = new win32cpp::Frame(NULL,win32cpp::WindowPadding(10));
    this->mainFrame->SetLayoutFlags(win32cpp::LayoutFillFill);
    win32cpp::LinearLayout *mainRowLayout   = new win32cpp::LinearLayout(win32cpp::VerticalLayout,win32cpp::LayoutFillFill);
    mainRowLayout->SetSpacing(10);


    // First a "Status" of the server
    win32cpp::LinearLayout *statusColumnLayout   = mainRowLayout->AddChild( new win32cpp::LinearLayout(win32cpp::HorizontalLayout) );
    win32cpp::Label *statusHeader = new win32cpp::Label(_T("Server status "));
    statusHeader->SetFont(boldFont);
    statusColumnLayout->AddChild( statusHeader );
    this->statusLabel   = statusColumnLayout->AddChild( new win32cpp::Label(_T("")));

    // Second a TabView for the settings
    win32cpp::ListView *usersList = mainRowLayout->AddChild( new win32cpp::ListView() );
    usersList->SetLayoutFlags(win32cpp::LayoutFillFill);
    this->listViewController    = new musik::server::ConnectedUsersListController(*usersList,this);


    this->mainFrame->AddChild(mainRowLayout);
    this->mainWindow.AddChild(mainFrame);

    // Connect size and position signals
    this->mainWindow.Resized.connect(this, &ConnectedUsersController::OnResize);
    this->mainWindow.Destroyed.connect(this, &ConnectedUsersController::OnDestroyed);

    // Start the status timer
    this->timer.OnTimeout.connect(this,&ConnectedUsersController::UpdateStatus);
    this->timer.ConnectToWindow(&this->mainWindow);
    this->timer.Start();

    this->server->UserSessionsUpdated.connect(this,&ConnectedUsersController::UpdateUserlist);

}

void ConnectedUsersController::OnResize(Window* window, Size size)
{
    RedrawLock redrawLock(this->mainFrame);
    this->mainFrame->Resize(this->mainWindow.ClientSize());
}

void ConnectedUsersController::OnDestroyed(Window* window)
{
    Point location( window->Location() );
    Size size( window->WindowSize() );
    musik::core::Preferences windowPrefs("ServerUsersWindow");
    windowPrefs.SetInt("x",location.x);
    windowPrefs.SetInt("y",location.y);
    windowPrefs.SetInt("width",size.width);
    windowPrefs.SetInt("height",size.height);
}

void ConnectedUsersController::UpdateStatus(){
    if(this->statusLabel && this->server){
        this->statusLabel->SetCaption( this->server->indexer.GetStatus() );
    }
}

void ConnectedUsersController::OnFileExit(MenuItemRef menuItem)
{
    Application::Instance().Terminate();
}

void ConnectedUsersController::OnSettings(MenuItemRef menuItem){
	static bool settingsShowing(false);

	if(!settingsShowing){
		settingsShowing	= true;

		this->mainWindow.Show(SW_SHOWNORMAL);

		TopLevelWindow dialog(_T("Settings"));
		MainWindowController settingsController(dialog,this->server);
		dialog.ShowModal(&this->mainWindow);

		this->mainWindow.Show(SW_HIDE);
		this->mainWindow.Show(SW_MINIMIZE);

		settingsShowing	= false;
	}
}

void ConnectedUsersController::UpdateUserlist(){
    if(!win32cpp::ApplicationThread::InMainThread()){
        win32cpp::ApplicationThread::Call0(this,&ConnectedUsersController::UpdateUserlist);
        return;
    }

    // Get the connected users
    musik::core::server::UserSessionVector userSessions( this->server->ConnectedUserSessions() );

}

