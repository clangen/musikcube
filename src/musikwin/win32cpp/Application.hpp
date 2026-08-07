//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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

/**
 * @file Application.hpp
 * @brief Singleton managing process-level Win32 application state.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. The
 * Application object records the module handles and command line passed
 * to WinMain, runs the main message loop, and owns the top-level window,
 * the ApplicationThread marshalling helper and the tray icon manager.
 */

#pragma once
#include <win32cpp/Types.hpp>
#include <win32cpp/TopLevelWindow.hpp>


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
///\details
///Wraps the raw HINSTANCE handles passed to WinMain, stores the command line
///and window show command, and owns the main message pump (Run). It also
///owns the process-wide ApplicationThread used to marshal cross-thread calls
///and the TrayIconManager for system tray icons.
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

public:
    ///\brief Constructs the singleton application object.
    /*ctor*/            Application();

public: // methods
    ///\brief Stores the parameters received by WinMain and initializes the
    ///application-level subsystems.
    ///\param instance the current instance handle of the process
    ///\param previousInstance the previously running instance handle (usually NULL)
    ///\param commandLine the command line string passed to WinMain
    ///\param showCommand how the initial window should be shown
    ///\note Must be called exactly once, from WinMain.
    static void         Initialize(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand);

    ///\brief Returns the sole Application instance.
    ///\return reference to the process-wide singleton
    static Application& Instance();

    ///\brief Displays the given window and runs the Win32 message loop.
    ///\param mainWindow the main top-level window of the application
    ///\note Blocks until the main window is closed.
    void                Run(TopLevelWindow& mainWindow);

    ///\brief Returns the current main window of the application.
    ///\return pointer to the main TopLevelWindow, or NULL if not set
    TopLevelWindow*     MainWindow();

    ///\brief Returns the previously running instance handle.
    ///\return the HINSTANCE passed as previousInstance to Initialize
    HINSTANCE           PreviousInstance() const;

    ///\brief Returns the command line passed to WinMain.
    ///\return the stored command line string
    const uistring&     CommandLine() const;

    ///\brief Returns the show command used for the initial window.
    ///\return the showCommand value passed to Initialize
    int                 ShowCommand() const;

    ///\brief Posts a quit message to the message loop, terminating the app.
    ///\note Does not force the loop to exit immediately.
    void                Terminate() const;

    ///\brief Returns the application's tray icon manager.
    ///\return pointer to the TrayIconManager, or NULL if not created
    TrayIconManager*    SysTrayManager() const;

    ///\brief Returns the ApplicationThread used for cross-thread calls.
    ///\return pointer to the thread marshalling helper
    ApplicationThread*  Thread();

public: // operator overloads
    ///\brief Implicit conversion to the current instance handle.
    operator HINSTANCE() const;

private: // methods
    void                OnMainWindowDestroyed(Window* window);

private: // instance data
    HINSTANCE instance;             ///< the current instance handle
    HINSTANCE previousInstance;     ///< the previous instance handle
    uistring commandLine;           ///< the command line passed to WinMain
    int showCommand;                ///< the initial window show command
    TopLevelWindow* mainWindow;     ///< the main window (NULL until Run)
    ApplicationThread* appThread;   ///< thread marshalling helper
    TrayIconManager* trayIconManager; ///< system tray icon manager

private: // class data
    static Application sMainApplication; ///< the single shared instance
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
