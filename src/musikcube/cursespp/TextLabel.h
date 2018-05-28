//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/Text.h>
#include <sigslot/sigslot.h>

namespace cursespp {
    class TextLabel :
#if (__clang_major__ == 7 && __clang_minor__ == 3)
        public cursespp::Window,
        public cursespp::IKeyHandler,
        public std::enable_shared_from_this<TextLabel> {
#else
        public cursespp::Window,
        public cursespp::IKeyHandler {
#endif
    public:
        sigslot::signal1<TextLabel*> Activated;

        TextLabel();
        virtual ~TextLabel();

        virtual void SetText(
            const std::string& value,
            const text::TextAlign alignment = text::AlignLeft);

        virtual std::string GetText() { return this->buffer; }
        virtual size_t Length() { return u8cols(this->buffer); }
        virtual void SetBold(bool bold);
        virtual bool IsBold() { return this->bold; }
        virtual void OnRedraw();

        virtual bool KeyPress(const std::string& key);
        virtual bool MouseEvent(const IMouseHandler::Event& event);

    private:
        std::string buffer;
        text::TextAlign alignment;
        bool bold;
    };
}
