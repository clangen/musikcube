#pragma once

namespace musik { namespace core { namespace sdk {
    /* distinguishes failures that streaming callers handle differently. */
    enum class HttpError {
        None,
        Cancelled,
        Certificate,
        Timeout,
        Connection,
        Partial,
        Other,
    };
} } }
