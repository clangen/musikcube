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
, thread(new ApplicationThread())
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

    this->thread->Initialize();

    //
    mainWindow.Destroyed.connect(this, &Application::OnMainWindowDestroyed);
    //
    mainWindow.Show(this->ShowCommand());

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0) > 0)
    {
        //(::IsDialogMessage(msg.hwnd, &msg) == 0);
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    delete this->thread;
    this->thread = NULL;

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