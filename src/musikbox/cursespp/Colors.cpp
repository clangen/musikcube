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

#include <stdafx.h>
#include "Colors.h"

using namespace cursespp;

Colors::Colors() {
}

void Colors::Init() {
    init_pair(BOX_COLOR_WHITE_ON_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(BOX_COLOR_RED_ON_BLUE, COLOR_RED, COLOR_BLUE);
    init_pair(BOX_COLOR_YELLOW_ON_BLUE, COLOR_YELLOW, COLOR_BLUE);
    init_pair(BOX_COLOR_BLACK_ON_GREY, COLOR_BLACK, COLOR_WHITE);
    init_pair(BOX_COLOR_BLACK_ON_GREEN, COLOR_BLACK, COLOR_GREEN);
    init_pair(BOX_COLOR_YELLOW_ON_BLACK, COLOR_YELLOW, -1);
    init_pair(BOX_COLOR_WHITE_ON_BLACK, COLOR_WHITE, -1);
    init_pair(BOX_COLOR_RED_ON_BLACK, COLOR_RED, -1);
    init_pair(BOX_COLOR_RED_ON_GREY, COLOR_RED, COLOR_WHITE);
    init_pair(BOX_COLOR_GREEN_ON_BLACK, COLOR_GREEN, -1);
    init_pair(BOX_COLOR_BLACK_ON_BLACK, COLOR_BLACK, -1);
    init_pair(BOX_COLOR_RED_ON_GREEN, COLOR_RED, COLOR_GREEN);
    init_pair(BOX_COLOR_BLACK_ON_YELLOW, COLOR_BLACK, COLOR_YELLOW);
}
