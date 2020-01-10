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

#include <stdafx.h>

#include "Rating.h"

#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/utfutil.h>
#include <app/util/PreferenceKeys.h>

#include <string>
#include <map>

using namespace musik::core;
using namespace musik::core::prefs;

static bool initialized = false;

namespace musik { namespace cube {

    static std::map<int, std::string> kRatingToSymbols = {
        { 0, kEmptyStar  + kEmptyStar  + kEmptyStar  + kEmptyStar  + kEmptyStar  },
        { 1, kFilledStar + kEmptyStar  + kEmptyStar  + kEmptyStar  + kEmptyStar  },
        { 2, kFilledStar + kFilledStar + kEmptyStar  + kEmptyStar  + kEmptyStar  },
        { 3, kFilledStar + kFilledStar + kFilledStar + kEmptyStar  + kEmptyStar  },
        { 4, kFilledStar + kFilledStar + kFilledStar + kFilledStar + kEmptyStar  },
        { 5, kFilledStar + kFilledStar + kFilledStar + kFilledStar + kFilledStar },
    };

    const std::string getRatingString(int value) {
        if (!initialized) {
            updateDefaultRatingSymbols();
        }
        return kRatingToSymbols.find(value)->second;
    }

    extern void updateDefaultRatingSymbols() {
        initialized = true;
        auto prefs = Preferences::ForComponent(components::Settings);
        std::string filled = prefs->GetString(musik::cube::prefs::keys::RatingPositiveChar, kFilledStar);
        if (!filled.size()) filled = kFilledStar;
        std::string empty = prefs->GetString(musik::cube::prefs::keys::RatingNegativeChar, kEmptyStar);
        if (!empty.size()) filled = kEmptyStar;
        filled = u8substr(filled, 0, 1);
        empty = u8substr(empty, 0, 1);
        kRatingToSymbols[0] = empty  + empty  + empty  + empty  + empty;
        kRatingToSymbols[1] = filled + empty  + empty  + empty  + empty;
        kRatingToSymbols[2] = filled + filled + empty  + empty  + empty;
        kRatingToSymbols[3] = filled + filled + filled + empty  + empty;
        kRatingToSymbols[4] = filled + filled + filled + filled + empty;
        kRatingToSymbols[5] = filled + filled + filled + filled + filled;
    }

} }