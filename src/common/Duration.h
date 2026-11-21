//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include <musikcore/support/NarrowCast.h>
#include <musikcore/utfutil.h>
#include <cmath>
#include <string>

namespace musik { namespace duration { namespace detail {

    template <typename N>
    inline std::string formatDuration(N seconds) {
        N mins = (seconds / 60);
        N secs = seconds - (mins * 60);
        return u8fmt("%d:%02d", narrow_cast<int>(mins), narrow_cast<int>(secs));
    }

    template <typename N>
    inline std::string formatDurationWithHours(N seconds) {
        N hours = (seconds / 3600);
        seconds -= hours * 3600;
        N mins = (seconds / 60);
        N secs = seconds - (mins * 60);
        return u8fmt("%d:%02d:%02d", narrow_cast<int>(hours), narrow_cast<int>(mins), narrow_cast<int>(secs));
    }

} } }

namespace musik { namespace duration {

    inline std::string Duration(int seconds) {
        return detail::formatDuration(seconds);
    }

    inline std::string Duration(size_t seconds) {
        return detail::formatDuration(seconds);
    }

    inline std::string Duration(double seconds) {
        return Duration((int) std::round(seconds));
    }

    inline std::string Duration(const std::string& str) {
        if (str.size()) {
            int seconds = std::stoi(str);
            return Duration(seconds);
        }

        return "0:00";
    }

    inline std::string DurationWithHours(size_t seconds) {
        if (seconds < 3600) {
            return detail::formatDuration(seconds);
        }
        return detail::formatDurationWithHours(seconds);
    }

} }
