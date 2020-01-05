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

#include "TrackRowRenderers.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <core/library/LocalLibraryConstants.h>
#include <core/support/Duration.h>

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::cube::TrackRowRenderers;
using namespace cursespp;

#define DIGITS(x) (x > 9 ? (int) log10((double) x) + 1 : 1)

static const int DURATION_COL_WIDTH = 5; /* 00:00 */

namespace AlbumSort {
    static const int TRACK_COL_WIDTH = 3;
    static const int ARTIST_COL_WIDTH = 17;

    static Renderer renderer = [](TrackPtr track, size_t index, size_t width, TrackNumType type) -> std::string {
        std::string trackNum;

        int trackColWidth = TRACK_COL_WIDTH;
        if (type == TrackNumType::Metadata) {
            trackNum = text::Align(
                track->GetString(constants::Track::TRACK_NUM),
                text::AlignRight,
                TRACK_COL_WIDTH);
        }
        else {
            trackColWidth = std::max(TRACK_COL_WIDTH, DIGITS(index + 1));
            trackNum = text::Align(std::to_string(index + 1), text::AlignRight, trackColWidth);
        }

        std::string duration = text::Align(
            musik::core::duration::Duration(track->GetString(constants::Track::DURATION)),
            text::AlignRight,
            DURATION_COL_WIDTH);

        std::string artist = text::Align(
            track->GetString(constants::Track::ARTIST),
            text::AlignLeft,
            ARTIST_COL_WIDTH);

        int titleWidth =
            (int) width -
            (int) trackColWidth -
            DURATION_COL_WIDTH -
            ARTIST_COL_WIDTH -
            (3 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(
            track->GetString(constants::Track::TITLE),
            text::AlignLeft,
            (int) titleWidth);

        return u8fmt(
            "%s   %s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
            duration.c_str(),
            artist.c_str());
    };
}

namespace NowPlaying {
    static const int TRACK_COL_WIDTH = 3;
    static const int ARTIST_COL_WIDTH = 14;
    static const int ALBUM_COL_WIDTH = 14;

    static Renderer renderer = [](TrackPtr track, size_t index, size_t width, TrackNumType type) -> std::string {
        size_t trackColWidth = std::max(TRACK_COL_WIDTH, DIGITS(index + 1));
        std::string trackNum = text::Align(std::to_string(index + 1), text::AlignRight, trackColWidth);

        std::string duration = text::Align(
            duration::Duration(track->GetString(constants::Track::DURATION)),
            text::AlignRight,
            DURATION_COL_WIDTH);

        std::string album = text::Align(
            track->GetString(constants::Track::ALBUM),
            text::AlignLeft,
            ALBUM_COL_WIDTH);

        std::string artist = text::Align(
            track->GetString(constants::Track::ARTIST),
            text::AlignLeft,
            ARTIST_COL_WIDTH);

        int titleWidth =
            (int) width -
            (int) trackColWidth -
            DURATION_COL_WIDTH -
            ALBUM_COL_WIDTH -
            ARTIST_COL_WIDTH -
            (4 * 3); /* 3 = spacing */

        titleWidth = std::max(0, titleWidth);

        std::string title = text::Align(
            track->GetString(constants::Track::TITLE),
            text::AlignLeft,
            (int) titleWidth);

        return u8fmt(
            "%s   %s   %s   %s   %s",
            trackNum.c_str(),
            title.c_str(),
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
