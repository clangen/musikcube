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

namespace musik { namespace cube { namespace prefs {

    namespace keys {
        extern const std::string DisableCustomColors;
        extern const std::string UsePaletteColors;
        extern const std::string FirstRunSettingsDisplayed;
        extern const std::string ColorTheme;
        extern const std::string InheritBackgroundColor;
        extern const std::string MinimizeToTray;
        extern const std::string StartMinimized;
        extern const std::string AutoUpdateCheck;
        extern const std::string LastAcknowledgedUpdateVersion;
        extern const std::string LastLibraryView;
        extern const std::string LastBrowseCategoryType;
        extern const std::string LastBrowseCategoryId;
        extern const std::string LastBrowseDirectoryRoot;
        extern const std::string LastCategoryFilter;
        extern const std::string LastTrackFilter;
        extern const std::string TrackSearchSortOrder;
        extern const std::string CategoryTrackListSortOrder;
        extern const std::string RatingPositiveChar;
        extern const std::string RatingNegativeChar;
        extern const std::string AutoHideCommandBar;
        extern const std::string DisableRatingColumn;
        extern const std::string AppQuitKey;
    }

} } }

