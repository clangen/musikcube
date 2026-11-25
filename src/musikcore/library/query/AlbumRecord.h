#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace musik { namespace core { namespace library { namespace query {

    /* one album result and the metadata fields carried by the query protocol. */
    struct AlbumRecord {
        int64_t id = 0;
        std::string value;
        std::string type;
        std::unordered_map<std::string, std::string> metadata;
    };

    inline std::string AlbumText(const AlbumRecord& album, const std::string& key) {
        const auto found = album.metadata.find(key);
        return found == album.metadata.end() ? std::string() : found->second;
    }

    inline int64_t AlbumInteger(const AlbumRecord& album, const std::string& key) {
        const auto value = AlbumText(album, key);
        try {
            return value.empty() ? 0 : std::stoll(value);
        }
        catch (const std::invalid_argument&) {
            return 0;
        }
        catch (const std::out_of_range&) {
            return 0;
        }
    }

} } } }
