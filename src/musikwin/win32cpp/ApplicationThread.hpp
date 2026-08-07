//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Daniel �nnerby
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
 * @file ApplicationThread.hpp
 * @brief Thread-safe mechanism to invoke methods on the main thread.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. ApplicationThread
 * marshals calls made from worker threads onto the main (UI) thread using
 * an invisible message-only window and the sigslot library. Because the
 * connection is dropped when the destination object is destroyed, a
 * queued call is never delivered to a dead object.
 */

#pragma once
#include <win32cpp/Application.hpp>
#include <win32cpp/Window.hpp>
#include <sigslot/sigslot.h>
#include <list>
#include <mutex>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///ApplicationThread is a class handling intercommunication to main thread.
///
///To make a call to the main thread you do like this:
///\code
///win32cpp::ApplicationThread::Call1(this /*pointer to object*/,&MyClass::MyMethod,argument1);
///\endcode
///
///The calls are using the sigslot library and are threadsafe. If the object
///is deleted before mainthread have the time to call it, the call will not be made.
///
///\remarks
///ApplicationThread has only one instance in the Application class (singleton)
///
///\see
///Application
//////////////////////////////////////////
class ApplicationThread
{
    friend class Application;

private: // types
    class CallClassBase;
    class HelperWindow;
    typedef std::shared_ptr<CallClassBase> CallClassPtr;
    typedef std::list<CallClassPtr> CallVector;

public: // ctor, dtor
    ///\brief Constructs the thread marshaller.
    ApplicationThread();
    ///\brief Destroys the marshaller and its helper window.
    ~ApplicationThread();

public: // methods
    ///\brief Determines whether the calling thread is the main UI thread.
    ///\return true if the current thread owns the application's message loop
    static bool InMainThread();

private: // methods
    void MainThreadCallback();
    void NotifyMainThread();
    void Initialize();
    void AddCall(CallClassBase *callClass);


private: // instance data
	///\brief
	///The applications thread id
	DWORD applicationThreadId;

	///\brief
	///mutex for protecting the calls
    std::mutex mutex;

    ///\brief
    ///A list of all the calls to be made
    CallVector calls;

    ///\brief
    ///instance of the HelperWindow. Created when the Initialize is called.
    HelperWindow *helperWindow;

private: // "Call" classes

	///\brief
    ///The HelperWindow is a message only Window (invisible) to help sending messages to main thread.
	class HelperWindow : public win32cpp::Window
    {
    public:
        HelperWindow();
        virtual HWND        Create(Window* parent);
        virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
        virtual void        OnCreated();
    };

    ///\brief
    ///A virtual base class for all CallClasses
    class CallClassBase
    {
    public:
        virtual ~CallClassBase() { };
        ///\brief Invokes the wrapped member method. Pure virtual.
        virtual void Call() = 0;
    };

    ///\brief Wraps a call to a member method taking no arguments.
    template<class DestinationType>
    class CallClass0 : public CallClassBase
    {
    public:
        sigslot::signal0<> signal;
        CallClass0(
            DestinationType* destinationObject,
            void (DestinationType::*memberMethod)())
        {
            this->signal.connect(destinationObject, memberMethod);
        };

        void Call()
        {
            this->signal();
        };
    };

    template<class DestinationType, class Arg1Type>
    class CallClass1 : public CallClassBase
    {
    public:
        sigslot::signal1<Arg1Type> signal;
        Arg1Type arg1mem;

        CallClass1(
            DestinationType* destinationObject,
            void (DestinationType::*memberMethod)(Arg1Type),
            Arg1Type &arg1)
        : arg1mem(arg1)
        {
            this->signal.connect(destinationObject,memberMethod);
        };

        void Call()
        {
            this->signal(this->arg1mem);
        };
    };

    template<class DestinationType, class Arg1Type, class Arg2Type>
    class CallClass2 : public CallClassBase
    {
    public:
        sigslot::signal2<Arg1Type, Arg2Type> signal;
        Arg1Type arg1mem;
        Arg2Type arg2mem;

        CallClass2(
            DestinationType* destinationObject,
            void (DestinationType::*memberMethod)(Arg1Type, Arg2Type),
            Arg1Type &arg1,
            Arg2Type &arg2)
        : arg1mem(arg1)
        , arg2mem(arg2)
        {
            this->signal.connect(destinationObject,memberMethod);
        };

        void Call()
        {
            this->signal(this->arg1mem,this->arg2mem);
        };
    };

public: // "Call" invocation
    ///\brief Queues a call to a member method with no arguments on the main thread.
    ///\param destinationObject the object whose member method will be invoked
    ///\param memberMethod pointer to the member method to call
    ///\note If destinationObject is destroyed before the call runs, the call is dropped.
    template<class DestinationType>
    static void Call0(
        DestinationType* destinationObject,
        void (DestinationType::*memberMethod)())
    {
        win32cpp::Application::Instance().Thread()->AddCall(
            new CallClass0<DestinationType>(
            destinationObject,
            memberMethod));
    };

    ///\brief Queues a call to a member method taking one argument on the main thread.
    ///\param destinationObject the object whose member method will be invoked
    ///\param memberMethod pointer to the member method to call
    ///\param arg1 the argument to pass (captured by reference)
    ///\note If destinationObject is destroyed before the call runs, the call is dropped.
    template<class DestinationType,class Arg1Type>
    static void Call1(
        DestinationType* destinationObject,
        void (DestinationType::*memberMethod)(Arg1Type),
        Arg1Type &arg1)
    {
        win32cpp::Application::Instance().Thread()->AddCall(
            new CallClass1<DestinationType, Arg1Type>(
            destinationObject,
            memberMethod,
            arg1));
    };

    ///\brief Queues a call to a member method taking two arguments on the main thread.
    ///\param destinationObject the object whose member method will be invoked
    ///\param memberMethod pointer to the member method to call
    ///\param arg1 the first argument to pass (captured by reference)
    ///\param arg2 the second argument to pass (captured by reference)
    ///\note If destinationObject is destroyed before the call runs, the call is dropped.
    template<class DestinationType, class Arg1Type, class Arg2Type>
    static void Call2(
        DestinationType* destinationObject,
        void (DestinationType::*memberMethod)(Arg1Type, Arg2Type),
        Arg1Type &arg1,
        Arg2Type &arg2)
    {
        win32cpp::Application::Instance().Thread()->AddCall(
            new CallClass2<DestinationType, Arg1Type, Arg2Type>(
            destinationObject,
            memberMethod,
            arg1,
            arg2));
    };
};

//////////////////////////////////////////////////////////////////////////////
}   // win32cpp
//////////////////////////////////////////////////////////////////////////////
