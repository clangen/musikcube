#ifndef MUSIKCORE_LOG_HPP
#define MUSIKCORE_LOG_HPP

#include <musikcore/log.h>

#include <string_view>

namespace musik { namespace core { namespace api {

    inline mc_result WriteLog(mc_log_level level, std::string_view tag, std::string_view text) {
        return mc_log_write(level, { tag.data(), tag.size() }, { text.data(), text.size() });
    }

} } }

#endif
