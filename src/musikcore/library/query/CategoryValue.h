#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace musik { namespace core { namespace library { namespace query {

    /* one category value in a local or remote query result. */
    struct CategoryValue {
        std::string value;
        int64_t id = 0;
        std::string type;
        std::optional<int32_t> sourceId;
    };

    using CategoryValues = std::vector<CategoryValue>;

} } } }
