#pragma once

#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <common/LegacyConstants.h>
#include <string>

namespace musik { namespace core { class Indexer; } }

namespace musik { namespace core { namespace plugin {

    /* owns a C indexer source while the indexer worker and cancellation thread use it. */
    class IndexerSource final {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(IndexerSource);

        IndexerSource(ffi::PluginRegistry::Factory<mc_indexer_source_factory> factory,
            const std::string& pluginId, const std::string& name, Indexer& indexer);
        ~IndexerSource();

        void OnBeforeScan();
        void OnAfterScan();
        sdk::ScanResult Scan(Indexer* writer, const char** paths, unsigned count);
        void ScanTrack(Indexer* writer, const char* externalId);
        bool NeedsTrackScan();
        void Interrupt();
        bool HasStableIds();
        int SourceId();
        bool Failed() const;
        void Abort();

    private:
        void DestroyInstance() noexcept;

        ffi::PluginRegistry::Factory<mc_indexer_source_factory> factory;
        Indexer& indexer;
        mc_preferences_host_api preferences;
        mc_plugin_services services;
        mc_indexer_source_host_api sourceHost;
        std::string dataDirectory;
        std::string pluginId;
        std::string sourceKey;
        void* instance { nullptr };
        int sourceId { 0 };
        bool stableIds { false };
        bool validateTracks { false };
        bool failed { false };
        mc_indexer_scan_decision decision { MC_INDEXER_SCAN_ROLLBACK };
    };

} } }
