#pragma once

namespace musik { namespace core {

    enum ScanResult {
        ScanCommit = 1,
        ScanRollback = 2
    };

    enum class MetadataState: int {
        NotLoaded = 0,
        Loading = 1,
        Loaded = 2,
        Missing = 3
    };

} }
