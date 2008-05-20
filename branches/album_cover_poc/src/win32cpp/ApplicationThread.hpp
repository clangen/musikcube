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

#pragma once

#include <sigslot/sigslot.h>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/Window.hpp>

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
    typedef boost::shared_ptr<CallClassBase> CallClassPtr;
    typedef std::list<CallClassPtr> CallVector;

public: // ctor, dtor
    ApplicationThread();
    ~ApplicationThread();

public: // methods
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
    boost::mutex mutex;

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
        virtual void Call() = 0;
    };

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
