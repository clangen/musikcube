#pragma once

namespace runtime_tests {
    enum class MigrationFailure { None, Completion, Metadata };

    bool checkServices();
    bool checkPluginDirectory();
    bool prepareMigration();
    bool resumeMigration(MigrationFailure failure);
    bool verifyMigrationComplete();
    bool checkVisualizerSession();
    bool checkStreamSdkContract();
    bool checkRegistry();
    bool checkStartup();
    bool checkReadIsolation();
    bool checkLockedReadShutdown();
    bool checkDatabaseFailure();
    bool checkOwner();
    bool checkFailure(bool caches = false);
}
