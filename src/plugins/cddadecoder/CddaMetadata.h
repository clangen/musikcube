#pragma once

#include "CddaStream.h"

#include <stop_token>
#include <map>
#include <string>

struct CddaMetadata {
    std::string album;
    std::string artist;
    std::string genre;
    std::map<unsigned, std::string> titles;
};

std::string CddaMusicBrainzDiscId(const CddaDiscInfo& disc);
bool ParseCddaMusicBrainzResponse(const std::string& response, size_t trackCount, CddaMetadata& metadata);

/* A failed lookup leaves the caller free to use local track labels. */
bool LookupCddaMetadata(const CddaDiscInfo& disc, std::stop_token interrupted, CddaMetadata& metadata);
