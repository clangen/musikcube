//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <string>

namespace cursespp {
    class INavigationKeys {
        public:
            virtual ~INavigationKeys() { }

            virtual bool Up(const std::string& key) = 0;
            virtual bool Down(const std::string& key) = 0;
            virtual bool Left(const std::string& key) = 0;
            virtual bool Right(const std::string& key) = 0;
            virtual bool Next(const std::string& key) = 0;
            virtual bool PageUp(const std::string& key) = 0;
            virtual bool PageDown(const std::string& key) = 0;
            virtual bool Home(const std::string& key) = 0;
            virtual bool End(const std::string& key) = 0;
            virtual bool Prev(const std::string& key) = 0;
            virtual bool Mode(const std::string& key) = 0;

            virtual std::string Up() = 0;
            virtual std::string Down() = 0;
            virtual std::string Left() = 0;
            virtual std::string Right() = 0;
            virtual std::string Next() = 0;
            virtual std::string PageUp() = 0;
            virtual std::string PageDown() = 0;
            virtual std::string Home() = 0;
            virtual std::string End() = 0;
            virtual std::string Prev() = 0;
            virtual std::string Mode() = 0;
    };
}
