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

/**
 * @file PreferenceKeys.h
 * @brief Names and default values of the UI-layer preference keys.
 * @details Declares the preference keys used by the application UI, grouped
 *          into the keys namespace for names and the defaults namespace for
 *          the small number of non-zero default values.
 */

#include <string>

namespace musik { namespace cube { namespace prefs {

    /**
     * @brief Names of the preference keys used by the UI layer.
     */
    namespace keys {
        extern const std::string DisableCustomColors;           /**< ignore custom color overrides */
        extern const std::string UsePaletteColors;              /**< use terminal palette colors */
        extern const std::string FirstRunSettingsDisplayed;     /**< the first run dialog was shown */
        extern const std::string ColorTheme;                    /**< the active color theme name */
        extern const std::string InheritBackgroundColor;        /**< inherit the terminal background */
        extern const std::string MinimizeToTray;                /**< minimize to the system tray */
        extern const std::string StartMinimized;                /**< start the app minimized */
        extern const std::string AutoUpdateCheck;               /**< check for updates automatically */
        extern const std::string LastAcknowledgedUpdateVersion; /**< last update version the user saw */
        extern const std::string LastLibraryView;               /**< the last visited library view */
        extern const std::string LastBrowseCategoryType;        /**< the last browse category type */
        extern const std::string LastBrowseCategoryId;          /**< the last browse category id */
        extern const std::string LastBrowseDirectoryRoot;       /**< the last browsed directory root */
        extern const std::string LastBrowseFilterVisible;       /**< whether the browse filter was visible */
        extern const std::string LastBrowseFilter;              /**< the last browse filter text */
        extern const std::string LastCategoryFilter;            /**< the last category filter text */
        extern const std::string LastCategoryFilterMatchType;   /**< the last category match type */
        extern const std::string LastTrackFilter;               /**< the last track filter text */
        extern const std::string LastTrackFilterMatchType;      /**< the last track match type */
        extern const std::string TrackSearchSortOrder;          /**< the track search sort order */
        extern const std::string CategoryTrackListSortOrder;    /**< the category track list sort order */
        extern const std::string RatingPositiveChar;            /**< the character for a filled rating star */
        extern const std::string RatingNegativeChar;            /**< the character for an empty rating star */
        extern const std::string AutoHideCommandBar;            /**< auto-hide the command bar */
        extern const std::string DisableRatingColumn;           /**< hide the rating column in lists */
        extern const std::string DisableWindowTitleUpdates;     /**< do not update the terminal title */
        extern const std::string AppQuitKey;                    /**< the key sequence that quits the app */
    }

    /**
     * @brief Default values for preference keys where the default differs
     *        from the zero value.
     */
    namespace defaults {
        extern const bool DisableWindowTitleUpdates;            /**< default: keep window title updates enabled */
    }

} } }

