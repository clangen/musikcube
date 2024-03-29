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

#include <stdafx.h>

#include "TrackRowRenderers.h"
#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/support/Duration.h>
#include <app/util/Rating.h>
#include <app/util/PreferenceKeys.h>
#include <math.h>

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::cube;
using namespace musik::cube::TrackRowRenderers;
using namespace cursespp;

#define DIGITS(x) (x > 9 ? (int) log10((double) x) + 1 : 1)

constexpr bool kEnableSkeletonRows = true;
constexpr int kDurationColWidth = 5; /* 00:00 */
constexpr int kRatingBreakpointWidth = 90;
static const std::string kSkeletonChar = "-"; // "░";

/* this method does a couple things slower than it probably should, but it
shouldn't cause any issues. TODO: make this better? does it matter? */
static inline std::string getRatingForTrack(TrackPtr track, size_t width) {
    if (width <= kRatingBreakpointWidth) {
        return "";
    }
    auto p = Preferences::ForComponent(musik::core::prefs::components::Settings);
    if (p->GetBool(musik::cube::prefs::keys::DisableRatingColumn, false)) {
        return "";
    }
    const int rating = std::max(0, std::min(5, track->GetInt32("rating", 0)));
    return "   " + getRatingString(rating);
}

static std::string placeholder(int width) {
    std::string result;
    for (int i = 0; i < width; i++) {
        result += kSkeletonChar;
    }
    return result;
}

namespace AlbumSort {
    constexpr int kTrackColWidth = 3;
    constexpr int kArtistColWidth = 17;
    constexpr int kRatingColumnWidth = 5;

    static std::string skeleton(TrackPtr track, size_t width) {
        auto const id = track->GetId();

        std::string rating = width <= kRatingBreakpointWidth ? "" : getRatingString(0);
        std::string trackNum = text::Align(placeholder(2), text::AlignRight, kTrackColWidth);
        std::string duration = text::Align("0:00", text::AlignRight, kDurationColWidth);
        std::string artist = text::Align(placeholder(1 + (id % (kArtistColWidth - 2))), text::AlignLeft, kArtistColWidth);

        int titleWidth =
            narrow_cast<int>(width) -
            narrow_cast<int>(kTrackColWidth) -
            kDurationColWidth -
            kArtistColWidth -
            narrow_cast<int>(u8len(rating)) -
            (3 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(placeholder(
            1 + (id % (static_cast<int64_t>(titleWidth) - 2))),
            text::AlignLeft,
            narrow_cast<int>(titleWidth));

        return u8fmt(
            "%s   %s%s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
            rating.c_str(),
            duration.c_str(),
            artist.c_str());
    }

    static const Renderer renderer = [](TrackPtr track, size_t index, size_t width, TrackNumType type) -> std::string {
        if (track->GetMetadataState() != musik::core::sdk::MetadataState::Loaded) {
            return kEnableSkeletonRows ? skeleton(track, width) : std::string(width, ' ');;
        }

        std::string trackNum;

        std::string rating = getRatingForTrack(track, width);

        int trackColWidth = kTrackColWidth;
        if (type == TrackNumType::Metadata) {
            trackNum = text::Align(
                track->GetString(constants::Track::TRACK_NUM),
                text::AlignRight,
                kTrackColWidth);
        }
        else {
            trackColWidth = std::max(kTrackColWidth, DIGITS(index + 1));
            trackNum = text::Align(std::to_string(index + 1), text::AlignRight, trackColWidth);
        }

        std::string duration = text::Align(
            musik::core::duration::Duration(track->GetString(constants::Track::DURATION)),
            text::AlignRight,
            kDurationColWidth);

        std::string artist = text::Align(
            track->GetString(constants::Track::ARTIST),
            text::AlignLeft,
            kArtistColWidth);

        int titleWidth = 0;

        titleWidth =
            narrow_cast<int>(width) -
            narrow_cast<int>(trackColWidth) -
            kDurationColWidth -
            kArtistColWidth -
            narrow_cast<int>(u8len(rating)) -
            (3 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(
            track->GetString(constants::Track::TITLE),
            text::AlignLeft,
            narrow_cast<int>(titleWidth));

        return u8fmt(
            "%s   %s%s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
            rating.c_str(),
            duration.c_str(),
            artist.c_str());
    };
}

namespace NowPlaying {
    constexpr int kTrackColWidth = 3;
    constexpr int kArtistColWidth = 14;
    constexpr int kAlbumColWidth = 14;

    static std::string skeleton(TrackPtr track, size_t width) {
        auto const id = track->GetId();

        std::string rating = width <= kRatingBreakpointWidth ? "" : getRatingString(0);
        std::string trackNum = text::Align(placeholder(2), text::AlignRight, kTrackColWidth);
        std::string duration = text::Align("0:00", text::AlignRight, kDurationColWidth);
        std::string album = text::Align(placeholder(1 + (id % (kAlbumColWidth - 2))), text::AlignLeft, kAlbumColWidth);
        std::string artist = text::Align(placeholder(1 + (id % (kArtistColWidth - 2))), text::AlignLeft, kArtistColWidth);

        int titleWidth =
            narrow_cast<int>(width) -
            narrow_cast<int>(kTrackColWidth) -
            kDurationColWidth -
            kAlbumColWidth -
            kArtistColWidth -
            narrow_cast<int>(u8len(rating)) -
            (4 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(placeholder(
            1 + (id % (static_cast<int64_t>(titleWidth) - 2))),
            text::AlignLeft,
            narrow_cast<int>(titleWidth));

        return u8fmt(
            "%s   %s%s   %s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
            rating.c_str(),
            duration.c_str(),
            album.c_str(),
            artist.c_str());
    }

    static const Renderer renderer = [](TrackPtr track, size_t index, size_t width, TrackNumType type) -> std::string {
        if (track->GetMetadataState() != musik::core::sdk::MetadataState::Loaded) {
            return kEnableSkeletonRows ? skeleton(track, width) : std::string(width, ' ');
        }

        const size_t trackColWidth = std::max(kTrackColWidth, DIGITS(index + 1));
        std::string trackNum = text::Align(std::to_string(index + 1), text::AlignRight, trackColWidth);
        std::string rating = getRatingForTrack(track, width);

        std::string duration = text::Align(
            duration::Duration(track->GetString(constants::Track::DURATION)),
            text::AlignRight,
            kDurationColWidth);

        std::string album = text::Align(
            track->GetString(constants::Track::ALBUM),
            text::AlignLeft,
            kAlbumColWidth);

        std::string artist = text::Align(
            track->GetString(constants::Track::ARTIST),
            text::AlignLeft,
            kArtistColWidth);

        int titleWidth =
            narrow_cast<int>(width) -
            narrow_cast<int>(trackColWidth) -
            kDurationColWidth -
            kAlbumColWidth -
            kArtistColWidth -
            narrow_cast<int>(u8cols(rating)) -
            (4 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(
            track->GetString(constants::Track::TITLE),
            text::AlignLeft,
            (int) titleWidth);

        return u8fmt(
            "%s   %s%s   %s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
            rating.c_str(),
            duration.c_str(),
            album.c_str(),
            artist.c_str());
    };
}

namespace musik {
    namespace cube {
        namespace TrackRowRenderers {
            const Renderer Get(Type type) {
                switch (type) {
                    case Type::AlbumSort: return AlbumSort::renderer;
                    case Type::NowPlaying: return NowPlaying::renderer;
                }
                throw std::runtime_error("invalid type specified to TrackRowRenderers::Get");
            }
        }
    }
}
