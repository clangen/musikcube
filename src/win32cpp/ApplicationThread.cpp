//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Daniel Önnerby
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

#include "ApplicationThread.hpp"

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
ApplicationThread::ApplicationThread()
: helperWindow(NULL)
{
    this->applicationThreadId = GetCurrentThreadId();
}

//////////////////////////////////////////
///\brief
///Destructor
///
///\remarks
///Will also delete the HelperWindow
//////////////////////////////////////////
ApplicationThread::~ApplicationThread()
{
    delete this->helperWindow;
}

//////////////////////////////////////////
///\brief
///Ask if the current caller is executed in the main thread
//////////////////////////////////////////
bool ApplicationThread::InMainThread()
{
    ApplicationThread *thread = Application::Instance().Thread();

    if(thread)
    {
        DWORD theThread = GetCurrentThreadId();
        return (thread->applicationThreadId == theThread);
    }

    return false;
}

//////////////////////////////////////////
///\brief
///Adds a call to the calls queue of calls.
//////////////////////////////////////////
void ApplicationThread::AddCall(CallClassBase *callClass)
{
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->calls.push_back(CallClassPtr(callClass));
    }
    this->NotifyMainThread();
}

//////////////////////////////////////////
///\brief
///Method called by the HelperWindow to execute the calls
//////////////////////////////////////////
void ApplicationThread::MainThreadCallback()
{
    boost::mutex::scoped_lock lock(this->mutex);

    while( ! this->calls.empty())
    {
        CallClassPtr currentCall = this->calls.front();
        currentCall->Call();

        this->calls.pop_front();
    }
}

//////////////////////////////////////////
///\brief
///Sends a message to the HelperWindow to notify that there are new calls
//////////////////////////////////////////
void ApplicationThread::NotifyMainThread()
{
    if(this->helperWindow)
    {
        ::PostMessage(this->helperWindow->Handle(), WM_W32CPP_APPLICATIONTHREAD_CALL_WAITING, NULL, NULL);
    }
}

//////////////////////////////////////////
///\brief
///Initialize the HelperWindow
//////////////////////////////////////////
void ApplicationThread::Initialize()
{
    // Create helper window
    this->helperWindow = new ApplicationThread::HelperWindow();
    this->helperWindow->Initialize();
}


//////////////////////////////////////////
///\brief
///Constructor for HelperWindow
//////////////////////////////////////////
ApplicationThread::HelperWindow::HelperWindow()
{
}

//////////////////////////////////////////
///\brief
///Create HelperWindow
///
/// The parent window is not needed since message windows
/// does not require this.
//////////////////////////////////////////
HWND ApplicationThread::HelperWindow::Create(Window* parent){
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                       // ExStyle
        _T("Message"),              // Class name
        _T("ThreadHelperWindow"),   // Window name
        style,                      // Style
        0,                          // X
        0,                          // Y
        0,                          // Width
        0,                          // Height
        HWND_MESSAGE,               // Parent
        NULL,                       // Menu
        hInstance,                  // Instance
        NULL);                      // lParam

    return hwnd;
}

//////////////////////////////////////////
///\brief
///HelperWindow message handler.
//////////////////////////////////////////
LRESULT ApplicationThread::HelperWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_W32CPP_APPLICATIONTHREAD_CALL_WAITING:
        // This is a ApplicationTread message
        ApplicationThread *thread = Application::Instance().Thread();
        if(thread)
        {
            thread->MainThreadCallback();
        }
        return 0;   // 0 = processed
    }

    return this->DefaultWindowProc(message, wParam, lParam);
}

//////////////////////////////////////////
///\brief
///This method is called when message window has been created
///
///If calls has been added to the calls queue
///before this, they have not been called.
///This method will make a call to the MainThreadCallback
//////////////////////////////////////////
void ApplicationThread::HelperWindow::OnCreated()
{
    ApplicationThread *thread = Application::Instance().Thread();

    if(thread)
    {
        thread->MainThreadCallback();
    }
}
