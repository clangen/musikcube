#include "CddaIndexer.h"
#include "CddaMediaStatus.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>

namespace {
    /* Preserve the legacy CDDB disc ID so a rescan updates existing entries. */
    std::string DiscId(const CddaDiscInfo& disc) {
        unsigned sum = 0;
        for (const auto& track : disc.tracks) {
            unsigned seconds = track.startFrame / 75;
            while (seconds) {
                sum += seconds % 10;
                seconds /= 10;
            }
        }

        const auto duration = disc.tracks.back().endFrame / 75 - disc.tracks.front().startFrame / 75;
        const uint32_t id = ((sum % 255) << 24) | (duration << 8) | static_cast<uint32_t>(disc.tracks.size());
        char text[9] {};
        std::snprintf(text, sizeof(text), "%08x", id);
        return text;
    }

    std::string ExternalId(const CddaDiscInfo& disc, const std::string& discId, size_t index) {
        const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(disc.driveLetter)));
        return "audiocd/" + std::string(1, letter) + "/" + discId + "/" + std::to_string(index);
    }
}

CddaIndexer::CddaIndexer(const mc_host_api&, const mc_plugin_services& services)
: mediaMonitor(std::make_unique<CddaMediaMonitor>(*services.indexer)) {
    /* Include discs already present when the source starts. */
    services.indexer->request_rescan(services.indexer->context);
}

CddaIndexer::~CddaIndexer() = default;

const std::string& CddaIndexer::Label() {
    static const std::string label = "Audio CDs";
    return label;
}

const std::string& CddaIndexer::Key() {
    static const std::string key = "CD Audio IDecoder, IDataStream";
    return key;
}

void CddaIndexer::OnBeforeScan() {
    this->interrupted.store(false);
    this->present.clear();
    this->uncertainDrives.clear();
}

void CddaIndexer::OnAfterScan() {
}

bool CddaIndexer::ReadDisc(char letter, CddaDiscInfo& disc, bool& uncertain) {
    uncertain = false;
    const auto changeSnapshot = this->mediaMonitor->ChangeSnapshot();
    char root[] { letter, ':', '\\', '\0' };
    const auto driveType = GetDriveTypeA(root);
    if (driveType != DRIVE_CDROM) {
        uncertain = CddaDriveTypeUncertain(driveType);
        return false;
    }

    /* Find an actual .cda entry rather than assuming every disc starts with
    track 1. The stream returns the complete TOC even for a data track. */
    std::string pattern(root);
    pattern += "Track*.cda";
    WIN32_FIND_DATAA data {};
    const HANDLE find = FindFirstFileA(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        /* No media or no audio track means this drive has no CDDA entries.
        Other errors are inconclusive and must not delete saved metadata. */
        uncertain = CddaFindFailureUncertain(error, this->mediaMonitor->MediaRemoved(letter));
        return false;
    }
    FindClose(find);

    std::string path(root);
    path += data.cFileName;
    /* The indexer needs only the TOC; reading audio here would move the drive
    head and compete with playback. */
    CddaStream stream;
    /* Clear the published pointer before the stack stream is destroyed,
    including when disc inspection throws. */
    auto clearActive = [this](CddaStream*) {
        std::lock_guard lock(this->activeMutex);
        this->active = nullptr;
    };
    std::unique_ptr<CddaStream, decltype(clearActive)> activeGuard(&stream, clearActive);
    {
        std::lock_guard lock(this->activeMutex);
        this->active = &stream;
        if (this->interrupted.load()) {
            stream.Interrupt();
        }
    }

    const auto result = stream.Inspect({ path.data(), path.size() });

    if ((result != MC_OK && result != MC_ERROR_UNSUPPORTED) || stream.Disc().tracks.empty()) {
        uncertain = true;
        return false;
    }
    disc = stream.Disc();
    this->mediaMonitor->ConfirmMediaPresent(letter, changeSnapshot);
    return true;
}

bool CddaIndexer::Scan(musik::core::plugin::IndexerWriter* writer, const char**, size_t) {
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (this->interrupted.load()) {
            return false;
        }
        CddaDiscInfo disc;
        bool uncertain = false;
        if (!this->ReadDisc(letter, disc, uncertain)) {
            if (uncertain) {
                this->uncertainDrives.insert(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
            }
            continue;
        }
        const auto discId = DiscId(disc);
        const std::string label = std::string("[audio cd ") + letter + ":\\]";

        /* Cache successful lookups for this source instance. A missing or
        unavailable online entry leaves the disc playable with local labels. */
        auto metadata = this->metadataCache.find(discId);
        if (metadata == this->metadataCache.end()) {
            CddaMetadata found;
            if (LookupCddaMetadata(disc, this->interrupted, found)) {
                metadata = this->metadataCache.emplace(discId, std::move(found)).first;
            }
        }
        if (this->interrupted.load()) {
            return false;
        }

        for (size_t i = 0; i < disc.tracks.size(); ++i) {
            if (this->interrupted.load()) {
                return false;
            }
            const auto& track = disc.tracks[i];
            if (!track.audio) {
                continue;
            }

            char path[32] {};
            std::snprintf(path, sizeof(path), "%c:\\%02u.cda", letter, track.number);
            std::string title = "track #" + std::to_string(track.number);
            std::string album = label;
            std::string artist = label;
            std::string genre = "Audio CD";
            if (metadata != this->metadataCache.end()) {
                if (!metadata->second.album.empty()) {
                    album = metadata->second.album;
                }
                if (!metadata->second.artist.empty()) {
                    artist = metadata->second.artist;
                }
                if (!metadata->second.genre.empty()) {
                    genre = metadata->second.genre;
                }
                const auto found = metadata->second.titles.find(track.number);
                if (found != metadata->second.titles.end() && !found->second.empty()) {
                    title = found->second;
                }
            }
            const auto duration = std::to_string((track.endFrame - track.startFrame) / 75);
            const auto number = std::to_string(track.number);
            const auto id = ExternalId(disc, discId, i);

            /* A transient lookup failure must not replace metadata already
            saved for this disc with generic labels. New tracks still get a
            usable local entry when the service is unavailable. */
            if (metadata == this->metadataCache.end() && writer->GetLastModifiedTime(id).has_value()) {
                this->present.insert(id);
                continue;
            }

            musik::core::plugin::IndexerRecord record(writer->Get());
            record.SetValue("album", album.c_str());
            record.SetValue("artist", artist.c_str());
            record.SetValue("album_artist", artist.c_str());
            record.SetValue("genre", genre.c_str());
            record.SetValue("filename", path);
            record.SetValue("title", title.c_str());
            record.SetValue("track", number.c_str());
            record.SetValue("duration", duration.c_str());
            writer->Save(record, id);
            this->present.insert(id);
        }
    }
    return !this->interrupted.load();
}

void CddaIndexer::ScanTrack(musik::core::plugin::IndexerWriter* writer, const char* externalId) {
    const std::string_view id(externalId ? externalId : "");
    const bool uncertain = id.size() >= 10 && id.substr(0, 8) == "audiocd/" && id[9] == '/' &&
        this->uncertainDrives.contains(id[8]);
    if (!this->interrupted.load() && !uncertain && this->present.find(externalId) == this->present.end()) {
        writer->RemoveByExternalId(externalId);
    }
}

void CddaIndexer::Interrupt() noexcept {
    this->interrupted.store(true);
    std::lock_guard lock(this->activeMutex);
    if (this->active) {
        this->active->Interrupt();
    }
}
