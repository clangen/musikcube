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

#pragma once

#include <win32cpp/Types.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/TopLevelWindow.hpp>

#include <boost/shared_ptr.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

// forward declaration
class ApplicationThread;

///\brief
///Application is a singleton that provides basic information about the
///current process.
///
///Application::Initialize should be called from the WinMain method, followed
///by Application::Run to show the main Window and start the message loop.
///
///Access Application's sole instance by using the Application::Instance method.
///
///\see
///TopLevelWindow
class Application : public EventHandler
{
public: // types
    ///\brief Thrown if Application::Initialize() is called more than once
    class ApplicationAlreadyInitializedException: public Exception { };

    ///\brief Thrown if Application::Run() is called while the application
    ///is already running.
    class ApplicationAlreadyRunningException: public Exception { };

public: // constructors
    /*ctor*/            Application();

public: // methods
    static void         Initialize(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand);
    static Application& Instance();

    void                Run(TopLevelWindow& mainWindow);
    TopLevelWindow*     MainWindow();
    HINSTANCE           PreviousInstance() const;
    const uistring&     CommandLine() const;
    int                 ShowCommand() const;
    void                Terminate() const;
    ApplicationThread*  Thread();

public: // operator overloads
    operator HINSTANCE() const;

private: // methods
    void                OnMainWindowDestroyed(Window* window);

private: // instance data
    HINSTANCE instance;
    HINSTANCE previousInstance;
    uistring commandLine;
    int showCommand;
    TopLevelWindow* mainWindow;
    ApplicationThread* appThread;

private: // class data
    static Application sMainApplication;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp