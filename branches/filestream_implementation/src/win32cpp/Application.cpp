//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: win32cpp
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
#include <win32cpp/Application.hpp>
#include <win32cpp/ApplicationThread.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// Application
//////////////////////////////////////////////////////////////////////////////

Application Application::sMainApplication;

/*ctor*/        Application::Application()
: instance(NULL)
, previousInstance(NULL)
, commandLine(_T(""))
, showCommand(NULL)
, mainWindow(NULL)
, appThread(NULL)
, trayIconManager(NULL)
{
}

///\brief
///Initialize the Application instance.
///
///This method is best called from WinMain.
///
///\param instance
///The HINSTANCE supplied to WinMain.
///
///\param previousInstance
///The "previous" HINSTANCE supplied to WinMain.
///
///\param commandLine
///The command line supplied to WinMain
///
///\param showCommand
///The show command supplied to WinMain.
///
///\throws AlreadyInitializedException
///This exception is thrown if Initialize() has already been called
///
///\see
///Application::Run
void            Application::Initialize(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand)
{
    if (sMainApplication.instance)
    {
        throw ApplicationAlreadyInitializedException();
    }

    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrls.dwICC = ICC_WIN95_CLASSES;
    ::InitCommonControlsEx(&initCtrls);

    sMainApplication.instance = instance;
    sMainApplication.previousInstance = previousInstance;
    sMainApplication.commandLine = commandLine;
    sMainApplication.showCommand = showCommand;
    sMainApplication.mainWindow = NULL;
}

///\brief
///Show the specified TopLevelWindow and runs the message pump.
///
///\param mainWindow
///The Application's main Window.
///
///\throws AlreadyRunningException
///AlreadyRunningException will be thrown if the Application instance is
///already runnning.
void            Application::Run(TopLevelWindow& mainWindow)
{
    if (this->mainWindow)
    {
        throw ApplicationAlreadyRunningException();
    }

    this->mainWindow = &mainWindow;

    if ( ! mainWindow)  // shortcut for IsInitialized()
    {
        mainWindow.Initialize();
    }

    this->appThread = new ApplicationThread();
    this->appThread->Initialize();

    this->trayIconManager = new TrayIconManager;

    mainWindow.Destroyed.connect(this, &Application::OnMainWindowDestroyed);
    //
    mainWindow.Show(this->ShowCommand());

    // start main message loop
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (msg.message == WM_QUIT)
        {
            ::PostQuitMessage(0);
            break;
        }

        /*if (::IsDialogMessage(this->windowHandle, &msg))
        {
        switch (msg.message)
        {
        case WM_PAINT: // 15
        OutputDebugStringA("WM_PAINT\n");
        break;
        case WM_KEYUP: // 257 (0x101)
        case WM_KEYFIRST: // 256 (0x100)
        this->OnRequestFocusNext();
        this->SendMessage(WM_NEXTDLGCTL, (WPARAM) Window::LastFocus(), TRUE);
        OutputDebugStringA((boost::format("focus: %1%\n") % Window::LastFocus()).str().c_str());
        OutputDebugStringA("WM_KEYUP\n");
        break;
        case WM_MOUSEFIRST: // 500 (0x200)
        OutputDebugStringA("WM_MOUSEFIRST\n");
        break;
        case WM_MOUSELEAVE: // 675 (0x2a3)
        OutputDebugStringA("WM_MOUSELEAVE\n");
        break;
        case WM_NCMOUSEMOVE: // 160 (0xa0)
        OutputDebugStringA("WM_NCMOUSEMOVE\n");
        break;
        case WM_TIMER: // 275 (0x113)
        OutputDebugStringA("WM_TIMER\n");
        break;
        case WM_NCMOUSELEAVE: // 274 (0x2a2)
        OutputDebugStringA("WM_NCMOUSELEAVE\n");
        break;
        case WM_LBUTTONDOWN: // 513 (0x201)
        OutputDebugStringA("WM_LBUTTONDOWN\n");
        break;
        case WM_LBUTTONUP: // 514 (0x202)
        OutputDebugStringA("WM_LBUTTONUP\n");
        break;
        //case WM_KEYFIRST: // 256 (0x100)
        //    this->OnRequestFocusNext();
        //    OutputDebugStringA("WM_KEYFIRST\n");
        //    break;
        default:
        OutputDebugStringA((boost::format("%1%\n") % msg.message).str().c_str());
        break;
        }

        continue;
        }*/

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    // end main message loop

    delete this->trayIconManager;
    this->trayIconManager = NULL;

    delete this->appThread;
    this->appThread = NULL;

    this->mainWindow = NULL;
}

void            Application::OnMainWindowDestroyed(Window* window)
{
    ::PostQuitMessage(0);
}

///\brief
///Returns the Application instance.
///
///\returns
///The Application instance.
Application&    Application::Instance()
{
    return Application::sMainApplication;
}

///\brief
///Returns a pointer to the main Window.
///
///If this value is NULL the application is not currently running.
TopLevelWindow*     Application::MainWindow()
{
    return this->mainWindow;
}

///\brief
///Returns the "previous" HINSTANCE specified in WinMain.
HINSTANCE           Application::PreviousInstance() const
{
    return this->previousInstance;
}

///\brief
///Returns the command line specified in WinMain.
const uistring&     Application::CommandLine() const
{
    return this->commandLine;
}

///\brief
///Returns the "show command" specified in WinMain.
int                 Application::ShowCommand() const
{
    return this->showCommand;
}

///\brief
///Terminates the application if it is running.
void                Application::Terminate() const
{
    if (this->mainWindow)
    {
        ::PostQuitMessage(0);
    }
}

///\brief
///Returns the Application instance's HINSTANCE.
Application::operator HINSTANCE() const
{
    return this->instance;
}

///\brief
///Returns the ApplicationThread associated with this application instance.
ApplicationThread* Application::Thread() 
{
    return this->appThread;
}

///\brief Returns the TrayIconManager
TrayIconManager* Application::SysTrayManager() const
{
    return this->trayIconManager;
}