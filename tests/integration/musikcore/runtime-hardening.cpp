#include "runtime-hardening-cases.h"
#include "../../support/check.h"

#include <string_view>

using namespace runtime_tests;

/* each case runs in its own process and isolated configuration root. */
int main(int argc, char** argv) {
    if (!TEST_CHECK(argc == 2)) {
        return 1;
    }
    const std::string_view mode(argv[1]);
    if (mode == "indexer_cancel") {
        return checkIndexerCancel() ? 0 : 1;
    }
    if (mode == "indexer_provider_cancel") {
        return checkIndexerProviderCancel() ? 0 : 1;
    }
#ifdef _WIN32
    if (mode == "windows_plugins") {
        return checkWindowsPlugins() ? 0 : 1;
    }
#endif
    if (mode == "stream_sdk_contract") {
        return checkStreamSdkContract() ? 0 : 1;
    }
    if (mode == "migration_prepare") {
        return prepareMigration() ? 0 : 1;
    }
    if (mode == "migration_fail_completion") {
        return resumeMigration(MigrationFailure::Completion) ? 0 : 1;
    }
    if (mode == "migration_fail_metadata") {
        return resumeMigration(MigrationFailure::Metadata) ? 0 : 1;
    }
    if (mode == "migration_resume") {
        return resumeMigration(MigrationFailure::None) ? 0 : 1;
    }
    if (mode == "migration_verify") {
        return verifyMigrationComplete() ? 0 : 1;
    }
    if (mode == "visualizer_session") {
        return checkVisualizerSession() ? 0 : 1;
    }
    if (mode == "plugin_directory") {
        return checkPluginDirectory() ? 0 : 1;
    }
    if (mode == "services") {
        return checkServices() ? 0 : 1;
    }
    if (mode == "registry") {
        return checkRegistry() ? 0 : 1;
    }
    if (mode == "startup") {
        return checkStartup() ? 0 : 1;
    }
    if (mode == "read_isolation") {
        return checkReadIsolation() ? 0 : 1;
    }
    if (mode == "locked_read_shutdown") {
        return checkLockedReadShutdown() ? 0 : 1;
    }
    if (mode == "database_failure") {
        return checkDatabaseFailure() ? 0 : 1;
    }
    if (mode == "owner") {
        return checkOwner() ? 0 : 1;
    }
    if (mode == "caches") {
        return checkFailure(true) ? 0 : 1;
    }
    if (mode == "failure") {
        return checkFailure() ? 0 : 1;
    }
    return 1;
}
