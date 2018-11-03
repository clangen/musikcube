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

#include "curses_config.h"

#define CURSESPP_DEFAULT_COLOR -1LL

#define CURSESPP_SELECTED_LIST_ITEM 1
#define CURSESPP_HIGHLIGHTED_LIST_ITEM 2
#define CURSESPP_HIGHLIGHTED_ERROR_LIST_ITEM 3
#define CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM 4
#define CURSESPP_LIST_ITEM_HEADER 5
#define CURSESPP_LIST_ITEM_HIGHLIGHTED_HEADER 6

#define CURSESPP_DEFAULT_CONTENT_COLOR 7
#define CURSESPP_DEFAULT_FRAME_COLOR 8
#define CURSESPP_FOCUSED_FRAME_COLOR 9

#define CURSESPP_TEXT_DEFAULT 10
#define CURSESPP_TEXT_DISABLED 11
#define CURSESPP_TEXT_FOCUSED 12
#define CURSESPP_TEXT_ACTIVE 13
#define CURSESPP_TEXT_WARNING 14
#define CURSESPP_TEXT_ERROR 15
#define CURSESPP_TEXT_HIDDEN 16

#define CURSESPP_BUTTON_NORMAL 17
#define CURSESPP_BUTTON_HIGHLIGHTED 18

#define CURSESPP_SHORTCUT_ROW_NORMAL 19
#define CURSESPP_SHORTCUT_ROW_FOCUSED 20

#define CURSESPP_OVERLAY_FRAME 21
#define CURSESPP_OVERLAY_CONTENT 22
#define CURSESPP_OVERLAY_INPUT_FRAME 23
#define CURSESPP_OVERLAY_TEXT_FOCUSED 24

#define CURSESPP_BANNER 25
#define CURSESPP_FOOTER 26

namespace cursespp {
    class Colors {
        private:
            Colors();

        public:
            enum Mode {
                RGB,
                Palette,
                Basic
            };

            enum BgType {
                Theme,
                Inherit,
            };

            static void Init(Mode mode = Mode::Basic, BgType bgType = BgType::Theme);
            static void SetTheme(const std::string& fn);
    };
}
