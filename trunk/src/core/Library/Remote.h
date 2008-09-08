//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

namespace musik{ namespace core{
    namespace Query{
        class Base;
    }
} }

#include <string>

#include <core/config.h>
#include <core/Library/Base.h>

#include <boost/asio.hpp>

//////////////////////////////////////////

namespace musik{ namespace core{ namespace Library{

//////////////////////////////////////////
///\brief
///Library used for your remote music.
//////////////////////////////////////////
class Remote : public Library::Base{
	private:
        Remote(utfstring identifier);
    public:
        // Methods:
		static LibraryPtr Create(utfstring identifier);
        ~Remote(void);

        bool Startup();
        utfstring GetInfo();
        virtual utfstring BasePath();

    protected:
        virtual void Exit();

    private:
        // Methods:
        void ReadThread();
        void WriteThread();

    private:
        // Variables:
        boost::asio::io_service ioService;
        boost::asio::ip::tcp::socket socket;

        std::string address;
        std::string port;
        std::string username;
        std::string password;
        std::string sessionId;

};
//////////////////////////////////////////
} } }
//////////////////////////////////////////

