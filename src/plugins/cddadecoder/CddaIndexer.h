#pragma once

#include "CddaStream.h"
#include "CddaMetadata.h"
#include "CddaMediaMonitor.h"
#include <musikcore/plugin/indexer.hpp>

#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <set>
#include <stop_token>
#include <string>

/* One source scans connected drives and owns the cancellation state for its
current device request. Its saved IDs include both drive and disc identity. */
class CddaIndexer {
public:
    CddaIndexer(const mc_host_api& host, const mc_plugin_services& services);
    ~CddaIndexer();

    static const std::string& Key();
    static const std::string& Label();
    void OnBeforeScan();
    void OnAfterScan();
    bool Scan(musik::core::plugin::IndexerWriter* writer, const char** paths, size_t count);
    void ScanTrack(musik::core::plugin::IndexerWriter* writer, const char* externalId);
    void Interrupt() noexcept;
    bool HasStableIds() const { return true; }
    bool NeedsTrackScan() const { return true; }

private:
    bool ReadDisc(char letter, CddaDiscInfo& disc, bool& uncertain);
    std::atomic<bool> interrupted { false };
    std::mutex activeMutex;
    std::stop_source metadataStop;
    CddaStream* active = nullptr;
    std::set<std::string> present;
    std::set<char> uncertainDrives;
    std::map<std::string, CddaMetadata> metadataCache;
    std::unique_ptr<CddaMediaMonitor> mediaMonitor;
};
