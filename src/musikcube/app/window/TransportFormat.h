#pragma once

#include <musikcore/playback.h>
#include <string>
#include <string_view>

namespace musik { namespace cube { namespace transport {

    inline constexpr char kStateToken[] = "$state";
    inline constexpr char kTitleToken[] = "$title";
    inline constexpr char kArtistToken[] = "$artist";
    inline constexpr char kAlbumToken[] = "$album";

    enum class Status { Stopped, Buffering, Paused, Playing };

    inline Status PlaybackStatus(mc_playback_state state, mc_playback_stream_state stream) {
        if (stream == MC_PLAYBACK_STREAM_BUFFERING) {
            return Status::Buffering;
        }
        if (state == MC_PLAYBACK_PAUSED || state == MC_PLAYBACK_PREPARED) {
            return Status::Paused;
        }
        if (stream == MC_PLAYBACK_STREAM_BUFFERED) {
            return Status::Buffering;
        }
        return state == MC_PLAYBACK_PLAYING ? Status::Playing : Status::Stopped;
    }

    inline const std::string& PlayingFormat(bool metadataReady, const std::string& fullFormat) {
        /* Separators belong to the metadata sentence. */
        static const std::string stateOnly { kStateToken };
        return metadataReady ? fullFormat : stateOnly;
    }

    struct FormatField {
        std::string_view text;
        int columns;
    };

    struct MetadataFields {
        FormatField title;
        FormatField artist;
        FormatField album;

        const FormatField* Find(std::string_view token) const {
            /* An empty field still resolves its directive while metadata is pending. */
            if (token == kTitleToken) {
                return &title;
            }
            if (token == kArtistToken) {
                return &artist;
            }
            if (token == kAlbumToken) {
                return &album;
            }
            return nullptr;
        }
    };

} } }
