#include "mpris.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <memory>
#include <unistd.h>

namespace {
    std::string field(const musik::core::api::TrackPage& page, const char* name) {
        for (size_t i = 0; i < page.FieldCount(); ++i) {
            if (page.FieldName(i) == name) {
                const auto value = page.Value(0, i);
                return std::string(value.data() ? value.data() : "", value.size());
            }
        }
        return {};
    }

    int64_t integer(const std::string& text) {
        int64_t value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() && result.ptr == text.data() + text.size() && value > 0 ? value : 0;
    }

    std::string fileUri(const std::string& path) {
        constexpr char hex[] = "0123456789ABCDEF";
        std::string result = "file://";
        for (const unsigned char ch : path) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                ch == '/' || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            {
                result.push_back(static_cast<char>(ch));
            }
            else {
                result.push_back('%');
                result.push_back(hex[ch >> 4]);
                result.push_back(hex[ch & 15]);
            }
        }
        return result;
    }

    struct ImageWrite {
        int fd = -1;
        bool written = false;
    };

    struct TemporaryArtwork {
        char path[32] = "/tmp/musikcube-mpris-XXXXXX";
        int fd = -1;
        bool keep = false;
        bool created = false;

        ~TemporaryArtwork() {
            if (this->fd >= 0) {
                close(this->fd);
            }
            if (this->created && !this->keep) {
                unlink(this->path);
            }
        }
    };

    void MC_CALL writeImage(mc_result status, mc_byte_view image, const mc_error*, void* data) noexcept {
        auto& output = *static_cast<ImageWrite*>(data);
        if (status != MC_OK || !image.data || !image.length) {
            return;
        }
        size_t offset = 0;
        while (offset < image.length) {
            const auto written = write(output.fd, image.data + offset, image.length - offset);
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                return;
            }
            offset += static_cast<size_t>(written);
        }
        output.written = true;
    }

}

void MPRISRemote::PlaybackReady(void* user, mc_result status, mc_playback* playback) noexcept {
    auto& output = *static_cast<MPRISRemote*>(user);
    musik::core::api::Playback owned(playback);
    if (status != MC_OK || !playback || output.stopping.load()) {
        return;
    }

    output.ownerPlayback.emplace(std::move(owned));
    if (output.currentTrackId > 0) {
        output.LoadMetadata(playback, output.generation.load(), output.currentTrackId);
    }
}

void MPRISRemote::LoadMetadata(mc_playback* playback, uint64_t serial, int64_t trackId) noexcept {
    auto& output = *this;
    try {
        mc_track_page* rawPage = nullptr;
        const auto status = mc_playback_get_current_track_metadata(playback, &rawPage);
        musik::core::api::TrackPage page(rawPage);
        if (status == MC_OK && page && page.Size() && page.Id(0) == trackId)
        {
            MPRISMetadataValues values;
            values.artist = field(page, MC_TRACK_FIELD_ARTIST);
            values.title = field(page, MC_TRACK_FIELD_TITLE);
            values.album = field(page, MC_TRACK_FIELD_ALBUM);
            values.albumArtist = field(page, MC_TRACK_FIELD_ALBUM_ARTIST);
            values.genre = field(page, MC_TRACK_FIELD_GENRE);
            values.trackNumber = static_cast<int32_t>(std::min<int64_t>(integer(field(page, MC_TRACK_FIELD_TRACK)), INT32_MAX));
            values.discNumber = static_cast<int32_t>(std::min<int64_t>(integer(field(page, MC_TRACK_FIELD_DISC)), INT32_MAX));
            const int64_t thumbnail = integer(field(page, MC_TRACK_FIELD_THUMBNAIL_ID));
            /* thumbnail IDs belong to their source library, including remote libraries. */
            std::shared_ptr<musik::core::api::Library> library;
            if (thumbnail) {
                mc_track_ref* rawTrack = nullptr;
                const auto trackStatus = mc_playback_get_current_track(playback, &rawTrack);
                musik::core::api::TrackRef track(rawTrack);
                if (trackStatus == MC_OK && track && track.Id() == trackId) {
                    try {
                        library = std::make_shared<musik::core::api::Library>(track.Origin());
                    }
                    catch (const musik::core::api::Error&) {
                        /* metadata remains useful if the originating library is unavailable. */
                    }
                }
            }

            {
                std::lock_guard lock(output.stateMutex);
                if (output.generation.load() == serial) {
                    values.available = true;
                    values.trackid = output.metadata.trackid;
                    output.metadata = std::move(values);
                    output.thumbnailId = thumbnail;
                    output.artworkLibrary = std::move(library);
                    output.artworkPending = true;
                    output.notifications.fetch_or(MetadataChanged | ArtworkChanged);
                }
            }
            output.artworkChanged.notify_one();
            output.Wake();
        }
    }
    catch (...) {
        output.Log("could not copy current track metadata");
    }
}

void MPRISRemote::RefreshArtwork() noexcept {
    try {
        int64_t id;
        uint64_t serial;
        std::shared_ptr<musik::core::api::Library> origin;
        {
            std::lock_guard lock(this->stateMutex);
            if (!this->artworkPending) {
                return;
            }
            this->artworkPending = false;
            id = this->thumbnailId;
            origin = this->artworkLibrary;
            serial = this->generation.load();
        }

        /* read on the worker so a remote catalog cannot block playback events. */
        std::string nextPath;
        std::string nextUri;
        TemporaryArtwork temporary;
        if (id > 0 && origin && this->library && this->library->read_library_thumbnail) {
            temporary.fd = mkstemp(temporary.path);
            temporary.created = temporary.fd >= 0;
            ImageWrite image { .fd = temporary.fd, .written = false };
            if (image.fd >= 0) {
                const auto status = this->library->read_library_thumbnail(
                    this->library->context, origin->Get(), id, writeImage, &image);
                const int closed = close(image.fd);
                temporary.fd = -1;
                if (status == MC_OK && image.written && closed == 0) {
                    nextPath = temporary.path;
                    nextUri = fileUri(nextPath);
                }
            }
        }

        std::string previous;
        {
            std::lock_guard lock(this->stateMutex);
            /* discard artwork fetched for a track that has since changed. */
            if (this->generation.load() != serial) {
                return;
            }
            previous.swap(this->artworkFile);
            this->artworkFile = std::move(nextPath);
            this->metadata.albumArt = std::move(nextUri);
            temporary.keep = !this->artworkFile.empty();
        }

        if (!previous.empty()) {
            unlink(previous.c_str());
        }
        this->notifications.fetch_or(MetadataChanged);
        this->Wake();
    }
    catch (...) {
        this->Log("could not prepare album artwork");
    }
}
