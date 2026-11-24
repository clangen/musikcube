#pragma once

#include <cstdint>
#include <string_view>

namespace musik { namespace core { namespace library {

    /* use the standard 32-bit FNV-1a offset basis and prime to keep source IDs
    stable across platforms and compiler upgrades. the catalog reserves zero
    for local files, so retain 31 bits and map zero to one. changing this
    calculation requires a catalog migration; the indexer rejects collisions. */
    inline int32_t IndexerSourceId(std::string_view plugin, std::string_view source) {
        uint32_t value = 2166136261u;
        for (const auto part : { plugin, std::string_view("/"), source }) {
            for (const unsigned char byte : part) {
                value = (value ^ byte) * 16777619u;
            }
        }
        value &= 0x7fffffffu;
        return static_cast<int32_t>(value ? value : 1u);
    }

} } }
