#include <musikcore/plugin/indexer.hpp>
#include <musikcore/plugin/registration.hpp>

#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>

/* a temporary factory expires before the host finishes using it. */
template<typename T, typename = void> struct CanRegisterEncoder : std::false_type {};
template<typename T> struct CanRegisterEncoder<T, std::void_t<decltype(
    musik::core::plugin::RegisterEncoder(std::declval<const mc_host_api&>(), std::declval<T>()))>> : std::true_type {};
static_assert(CanRegisterEncoder<mc_encoder_factory&>::value);
static_assert(!CanRegisterEncoder<mc_encoder_factory&&>::value);

/* record which host was passed to each independently constructed source. */
struct TestSource {
    explicit TestSource(const mc_host_api& host, const mc_plugin_services&) : host(&host) {}
    static const std::string& Key() { static const std::string key = "test"; return key; }
    static const std::string& Label() {
        static const std::string label = "Test music";
        return label;
    }
    void OnBeforeScan() {}
    void OnAfterScan() {}
    bool Scan(musik::core::plugin::IndexerWriter*, const char**, size_t) { return true; }
    void ScanTrack(musik::core::plugin::IndexerWriter*, const char*, int64_t modified) {
        this->lastModified = modified;
    }
    void Interrupt() {}
    bool HasStableIds() const { return true; }
    bool NeedsTrackScan() const { return false; }
    const mc_host_api* host;
    int64_t lastModified { 0 };
};

/* capture registration without copying its plugin-owned table. */
static mc_result MC_CALL capture(void* context, const mc_plugin_capability* capability) {
    *static_cast<const mc_indexer_source_factory**>(context) =
        static_cast<const mc_indexer_source_factory*>(capability->api);
    return MC_OK;
}

/* registration failures are returned to plugin initialization unchanged. */
static mc_result MC_CALL reject(void*, const mc_plugin_capability*) {
    return MC_ERROR_INVALID_ARGUMENT;
}

/* two sessions may use the same generated factory without sharing their hosts. */
int main() {
    /* malformed selectors must not partially overwrite caller state. */
    for (const char* invalid : { "gme://1junk/file.nsf", "gme://-1/file.nsf", "gme://1/",
             "gme://999999999999999999999/file.nsf" })
    {
        std::string path = "unchanged";
        int track = 42;
        if (musik::core::plugin::ParseExternalId("gme", invalid, path, track) ||
            path != "unchanged" || track != 42)
        {
            return EXIT_FAILURE;
        }
    }
    std::string path;
    int track = -1;
    if (!musik::core::plugin::ParseExternalId("gme", "gme://12/C:\\music\\file.nsf", path, track) ||
        path != "C:\\music\\file.nsf" || track != 12)
    {
        return EXIT_FAILURE;
    }
    const mc_indexer_source_factory* firstFactory = nullptr;
    const mc_indexer_source_factory* secondFactory = nullptr;
    mc_host_api first = MC_HOST_API_DEFAULT;
    mc_host_api second = MC_HOST_API_DEFAULT;
    first.host_context = &firstFactory;
    second.host_context = &secondFactory;
    first.register_plugin_capability = capture;
    second.register_plugin_capability = capture;
    mc_host_api rejecting = MC_HOST_API_DEFAULT;
    rejecting.register_plugin_capability = reject;
    if (musik::core::plugin::RegisterIndexerSource<TestSource>(rejecting) != MC_ERROR_INVALID_ARGUMENT) {
        return EXIT_FAILURE;
    }
    if (musik::core::plugin::RegisterIndexerSource<TestSource>(first) != MC_OK ||
        musik::core::plugin::RegisterIndexerSource<TestSource>(second) != MC_OK) {
        return EXIT_FAILURE;
    }
    mc_plugin_services firstServices = MC_PLUGIN_SERVICES_DEFAULT;
    mc_plugin_services secondServices = MC_PLUGIN_SERVICES_DEFAULT;
    firstServices.host = &first;
    secondServices.host = &second;
    void* firstSource = nullptr;
    void* secondSource = nullptr;
    if (!firstFactory || !secondFactory ||
        firstFactory->create(firstFactory->context, &firstServices, &firstSource, nullptr) != MC_OK ||
        secondFactory->create(secondFactory->context, &secondServices, &secondSource, nullptr) != MC_OK) {
        return EXIT_FAILURE;
    }
    mc_indexer_source_info truncated = MC_INDEXER_SOURCE_INFO_DEFAULT;
    truncated.struct_size = offsetof(mc_indexer_source_info, display_name);
    if (firstFactory->instance_api->get_info(firstSource, nullptr) != MC_ERROR_INVALID_ARGUMENT ||
        firstFactory->instance_api->get_info(firstSource, &truncated) != MC_ERROR_INVALID_ARGUMENT ||
        truncated.source_key.length != 0)
    {
        return EXIT_FAILURE;
    }
    mc_indexer_source_info info = MC_INDEXER_SOURCE_INFO_DEFAULT;
    if (firstFactory->instance_api->get_info(firstSource, &info) != MC_OK ||
        std::string(info.display_name.data, info.display_name.length) != "Test music")
    {
        return EXIT_FAILURE;
    }
    mc_indexer_writer writer = MC_INDEXER_WRITER_DEFAULT;
    const bool valid = firstFactory->instance_api->validate_track(firstSource, { "fixture", 7 },
        -12345, &writer, nullptr) == MC_OK && static_cast<TestSource*>(firstSource)->lastModified == -12345;
    const bool correct = valid && static_cast<TestSource*>(firstSource)->host == &first &&
        static_cast<TestSource*>(secondSource)->host == &second;
    firstFactory->instance_api->destroy(firstSource);
    secondFactory->instance_api->destroy(secondSource);
    return correct ? EXIT_SUCCESS : EXIT_FAILURE;
}
