//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#define BOX_COLOR_WHITE_ON_BLUE 1
#define BOX_COLOR_RED_ON_BLUE 2
#define BOX_COLOR_YELLOW_ON_BLUE 3
#define BOX_COLOR_BLACK_ON_GREY 4
#define BOX_COLOR_BLACK_ON_GREEN 5
#define BOX_COLOR_YELLOW_ON_BLACK 6
#define BOX_COLOR_WHITE_ON_BLACK 7
#define BOX_COLOR_RED_ON_BLACK 8
#define BOX_COLOR_RED_ON_GREY 9
#define BOX_COLOR_GREEN_ON_BLACK 10
#define BOX_COLOR_BLACK_ON_BLACK 11
#define BOX_COLOR_RED_ON_GREEN 12
#define BOX_COLOR_BLACK_ON_YELLOW 13

namespace cursespp {
    class Colors {
        private:
            Colors();

        public:
            static void Init();
    };
}
