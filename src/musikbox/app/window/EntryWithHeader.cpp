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
#include "EntryWithHeader.h"
#include <boost/algorithm/string.hpp>
#include <cursespp/Text.h>

using namespace musik::box;
using namespace cursespp;

EntryWithHeader::EntryWithHeader(
    const std::string& header, const std::string& value)
{
    this->header = header;
    this->value = value;
    this->attrs = -1;
}

void EntryWithHeader::SetWidth(size_t width) {
    this->width = width;
}

int64 EntryWithHeader::GetAttrs(size_t line) {
    return (line == 0) ? this->headerAttrs : this->attrs;
}

void EntryWithHeader::SetAttrs(int64 headerAttrs, int64 attrs) {
    this->headerAttrs = headerAttrs;
    this->attrs = attrs;
}

size_t EntryWithHeader::GetLineCount() {
    return 2;
}

#define TRUNCATE(value, width) \
    u8substr(value, 0, width > 0 ? width : 0)

std::string EntryWithHeader::GetLine(size_t line) {
    if (line == 0) {
        return text::Ellipsize(this->header, this->width);
    }

    return TRUNCATE(this->value, this->width);
}
