#pragma once

#include <string>
#include <vector>
#include <sigslot/sigslot.h>

namespace musik { namespace core {
    class IIndexer {
        public:
            sigslot::signal0<> SynchronizeStart;
            sigslot::signal0<> SynchronizeEnd;
            sigslot::signal0<> PathsUpdated;
            sigslot::signal0<> TrackRefreshed;

            virtual ~IIndexer() = 0 { }

            virtual void AddPath(const std::string& path) = 0;
            virtual void RemovePath(const std::string& path) = 0;
            virtual void GetPaths(std::vector<std::string>& paths) = 0;
            virtual void Synchronize(bool restart = false) = 0;
    };
} }