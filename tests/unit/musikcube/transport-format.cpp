#include <app/window/TransportFormat.h>

#include <cstdio>
#include <initializer_list>

using namespace musik::cube::transport;

int main() {
    if (PlaybackStatus(MC_PLAYBACK_STOPPED, MC_PLAYBACK_STREAM_BUFFERING) != Status::Buffering ||
        PlaybackStatus(MC_PLAYBACK_PLAYING, MC_PLAYBACK_STREAM_BUFFERING) != Status::Buffering ||
        PlaybackStatus(MC_PLAYBACK_PLAYING, MC_PLAYBACK_STREAM_BUFFERED) != Status::Buffering ||
        PlaybackStatus(MC_PLAYBACK_STOPPED, MC_PLAYBACK_STREAM_BUFFERED) != Status::Buffering ||
        PlaybackStatus(MC_PLAYBACK_PLAYING, MC_PLAYBACK_STREAM_PLAYING) != Status::Playing ||
        PlaybackStatus(MC_PLAYBACK_PREPARED, MC_PLAYBACK_STREAM_BUFFERED) != Status::Paused ||
        PlaybackStatus(MC_PLAYBACK_PAUSED, MC_PLAYBACK_STREAM_PLAYING) != Status::Paused ||
        PlaybackStatus(MC_PLAYBACK_STOPPED, MC_PLAYBACK_STREAM_DESTROYED) != Status::Stopped)
    {
        std::fprintf(stderr, "Presentation state disagrees with playback and stream state\n");
        return 1;
    }

    const std::string fullFormat { "$state $title by $artist from $album" };
    if (PlayingFormat(false, fullFormat) != kStateToken || PlayingFormat(true, fullFormat) != fullFormat) {
        std::fprintf(stderr, "Pending metadata must render playback state alone\n");
        return 1;
    }

    const MetadataFields pending {};
    for (const auto* token : { kTitleToken, kArtistToken, kAlbumToken }) {
        const auto* field = pending.Find(token);
        if (!field || !field->text.empty() || field->columns != 0) {
            std::fprintf(stderr, "Pending metadata did not resolve to an empty field: %s\n", token);
            return 1;
        }
    }

    const MetadataFields loaded {
        { "$Foxey Lady", 11 }, { "Jimi Hendrix", 12 }, { "Are You Experienced", 19 }
    };
    const auto* title = loaded.Find(kTitleToken);
    if (!title || title->text != "$Foxey Lady" || title->columns != 11 ||
        loaded.Find(kArtistToken) != &loaded.artist || loaded.Find(kAlbumToken) != &loaded.album ||
        loaded.Find("$unknown"))
    {
        std::fprintf(stderr, "Metadata lookup changed a literal value or matched an unknown directive\n");
        return 1;
    }

    return 0;
}
