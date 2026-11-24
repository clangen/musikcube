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
    void OnBeforeScan() {}
    void OnAfterScan() {}
    bool Scan(musik::core::plugin::IndexerWriter*, const char**, size_t) { return true; }
    void ScanTrack(musik::core::plugin::IndexerWriter*, const char*) {}
    void Interrupt() {}
    bool HasStableIds() const { return true; }
    bool NeedsTrackScan() const { return false; }
    const mc_host_api* host;
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
    const bool correct = static_cast<TestSource*>(firstSource)->host == &first &&
        static_cast<TestSource*>(secondSource)->host == &second;
    firstFactory->instance_api->destroy(firstSource);
    secondFactory->instance_api->destroy(secondSource);
    return correct ? EXIT_SUCCESS : EXIT_FAILURE;
}
