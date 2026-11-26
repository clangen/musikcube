#include "CddaMetadata.h"

#include <bcrypt.h>
#include <common/HttpClient.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
    using Clock = std::chrono::steady_clock;
    std::mutex rateMutex;
    std::condition_variable_any rateChanged;
    Clock::time_point lastRequest;

    /* MusicBrainz hashes the leadout and all 99 track offsets as uppercase
    hexadecimal text, then uses a URL-safe variant of Base64 for the digest. */
    std::string DiscId(const CddaDiscInfo& disc) {
        if (disc.tracks.empty() || disc.tracks.front().number == 0 || disc.tracks.back().number > 99) {
            return {};
        }

        /* Enhanced/mixed-mode discs need special leadout handling. Avoid
        presenting a potentially incorrect match until that is implemented. */
        if (std::any_of(disc.tracks.begin(), disc.tracks.end(),
                [](const auto& track) {
                    return !track.audio;
                }))
        {
            return {};
        }

        char value[9] {};
        std::string input;
        std::snprintf(value, sizeof(value), "%02X", disc.tracks.front().number);
        input += value;
        std::snprintf(value, sizeof(value), "%02X", disc.tracks.back().number);
        input += value;
        std::snprintf(value, sizeof(value), "%08X", disc.tracks.back().endFrame);
        input += value;

        size_t index = 0;
        for (unsigned number = 1; number <= 99; ++number) {
            uint32_t offset = 0;
            if (index < disc.tracks.size() && disc.tracks[index].number == number) {
                offset = disc.tracks[index++].startFrame;
            }
            std::snprintf(value, sizeof(value), "%08X", offset);
            input += value;
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0) {
            return {};
        }
        std::array<unsigned char, 20> digest {};
        const auto status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(input.data()),
            static_cast<ULONG>(input.size()), digest.data(), static_cast<ULONG>(digest.size()));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0) {
            return {};
        }

        constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
        std::string result;
        result.reserve(28);
        for (size_t i = 0; i < digest.size(); i += 3) {
            const uint32_t group = (uint32_t(digest[i]) << 16) |
                (i + 1 < digest.size() ? uint32_t(digest[i + 1]) << 8 : 0) |
                (i + 2 < digest.size() ? digest[i + 2] : 0);
            result.push_back(alphabet[(group >> 18) & 63]);
            result.push_back(alphabet[(group >> 12) & 63]);
            result.push_back(i + 1 < digest.size() ? alphabet[(group >> 6) & 63] : '-');
            result.push_back(i + 2 < digest.size() ? alphabet[group & 63] : '-');
        }
        return result;
    }

    struct Response {
        std::string body;
        bool good { true };

        Response& write(const char* data, size_t size) {
            if (size > 1024 * 1024 - this->body.size()) {
                this->good = false;
            }
            else {
                this->body.append(data, size);
            }
            return *this;
        }
        explicit operator bool() const { return this->good; }
    };

    std::string Artist(const nlohmann::json& credits) {
        std::string artist;
        if (!credits.is_array()) {
            return artist;
        }
        for (const auto& item : credits) {
            if (item.is_object()) {
                artist += item.value("name", std::string());
                artist += item.value("joinphrase", std::string());
            }
        }
        return artist;
    }

    std::string Genre(const nlohmann::json& entity) {
        if (!entity.is_object() || !entity.contains("genres") || !entity["genres"].is_array()) {
            return {};
        }

        /* MusicBrainz returns vote counts; use the most-supported genre. */
        std::string result;
        int highestCount = 0;
        for (const auto& genre : entity["genres"]) {
            if (!genre.is_object()) {
                continue;
            }
            const auto name = genre.value("name", std::string());
            const int count = genre.value("count", 0);
            if (!name.empty() && count > highestCount) {
                highestCount = count;
                result = name;
            }
        }
        return result;
    }
}

std::string CddaMusicBrainzDiscId(const CddaDiscInfo& disc) {
    return DiscId(disc);
}

bool LookupCddaMetadata(const CddaDiscInfo& disc, std::stop_token interrupted, CddaMetadata& metadata) {
    const auto id = CddaMusicBrainzDiscId(disc);
    if (id.empty() || interrupted.stop_requested()) {
        return false;
    }

    const auto url = "https://musicbrainz.org/ws/2/discid/" + id +
        "?inc=artists%2Brecordings%2Brelease-groups%2Bgenres&fmt=json";
    std::string response;
    using Client = musik::core::sdk::HttpClient<Response>;
    using Error = musik::core::sdk::HttpError;
    bool fetched = false;
    for (unsigned attempt = 0; attempt < 2 && !interrupted.stop_requested(); ++attempt) {
        /* MusicBrainz asks clients to make at most one request per second. */
        {
            std::unique_lock lock(rateMutex);
            rateChanged.wait_until(lock, interrupted, lastRequest + std::chrono::seconds(1), [] { return false; });
            if (interrupted.stop_requested()) {
                return false;
            }
            lastRequest = Clock::now();
        }

        auto client = Client::Create(Response {});
        int httpStatus = 0;
        bool success = false;
        client->Url(url).UserAgent("musikcube/1.0 (https://musikcube.com)")
            .Mode(Client::Thread::Current).ConnectTimeout(4000).Timeout(8000)
            .VerifyTls(true).Cancellation(interrupted)
            .Run([&](Client*, int status, bool ok) { httpStatus = status; success = ok; });
        if (success) {
            response = client->Stream().body;
            fetched = true;
            break;
        }

        /* Retry temporary service or connection failures once. A not-found
        response is a real no-match and should not cause repeated requests. */
        const bool retryable = httpStatus == 429 || httpStatus >= 500 ||
            client->Error() == Error::Connection || client->Error() == Error::Timeout;
        if (!retryable) {
            break;
        }
    }
    if (!fetched || interrupted.stop_requested()) {
        return false;
    }

    return ParseCddaMusicBrainzResponse(response, disc.tracks.size(), metadata);
}

bool ParseCddaMusicBrainzResponse(const std::string& response, size_t trackCount, CddaMetadata& metadata) {
    try {
        const auto json = nlohmann::json::parse(response);
        if (!json.contains("releases") || !json["releases"].is_array()) {
            return false;
        }
        for (const auto& release : json["releases"]) {
            if (!release.contains("media") || !release["media"].is_array()) {
                continue;
            }
            /* The disc-ID endpoint returns matching releases but omits their
            medium disc IDs. Only choose a medium when the track count selects
            one unambiguously; otherwise keep local labels. */
            const nlohmann::json* selected = nullptr;
            for (const auto& medium : release["media"]) {
                if (medium.value("track-count", 0u) != trackCount || !medium.contains("tracks") ||
                    !medium["tracks"].is_array())
                {
                    continue;
                }
                if (selected) {
                    selected = nullptr;
                    break;
                }
                selected = &medium;
            }
            if (selected) {
                const auto& medium = *selected;
                CddaMetadata found;
                found.album = release.value("title", std::string());
                if (release.contains("artist-credit")) {
                    found.artist = Artist(release["artist-credit"]);
                }
                found.genre = Genre(release);
                if (found.genre.empty() && release.contains("release-group")) {
                    found.genre = Genre(release["release-group"]);
                }
                for (const auto& track : medium["tracks"]) {
                    if (track.is_object() && track.contains("position") && track["position"].is_number_unsigned()) {
                        found.titles[track["position"].get<unsigned>()] = track.value("title", std::string());
                    }
                }
                metadata = std::move(found);
                return true;
            }
        }
    }
    catch (const nlohmann::json::exception&) {
        return false;
    }
    return false;
}
