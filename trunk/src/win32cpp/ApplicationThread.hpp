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

class ApplicationThread{
    protected:  
        friend class Application;
        friend class Application::HelperWindow;

                ApplicationThread(void);
    public:    ~ApplicationThread(void);
    protected: void MainThreadCallback();
    protected: void NotifyMainThread();

    protected: boost::mutex mutex;

    //////////////////////////////////////////////////////////////////////////////
    protected:  
        class CallClassBase{
            public: 
                virtual ~CallClassBase(void){};
                virtual void Call()=0;
        };
    //////////////////////////////////////////////////////////////////////////////
    protected:    
        typedef boost::shared_ptr<CallClassBase> CallClassPtr;
        typedef std::list<CallClassPtr> CallVector;

        CallVector calls;

        void AddCall(CallClassBase *callClass);

    //////////////////////////////////////////////////////////////////////////////
    protected:
        template<class DestinationType> 
        class CallClass0 : public CallClassBase{
            public:
                sigslot::signal0<> signal;
                CallClass0(DestinationType* destinationObject,void (DestinationType::*memberMethod)()){
                    this->signal.connect(destinationObject,memberMethod);
                };

                void Call(){
                    this->signal();
                };
        };
    //////////////////////////////////////////////////////////////////////////////
    public:     
        template<class DestinationType> 
        static void Call0(DestinationType* destinationObject,void (DestinationType::*memberMethod)()){
            win32cpp::Application::Instance().thread->AddCall(new CallClass0<DestinationType>(destinationObject,memberMethod));    
        };
    //////////////////////////////////////////////////////////////////////////////

    private:
};


//////////////////////////////////////////////////////////////////////////////
}   // win32cpp
//////////////////////////////////////////////////////////////////////////////
