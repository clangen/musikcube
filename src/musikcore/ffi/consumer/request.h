#ifndef MUSIKCORE_FFI_CONSUMER_REQUEST_H
#define MUSIKCORE_FFI_CONSUMER_REQUEST_H

#include <musikcore/context.h>
#include <musikcore/player.h>
#include <musikcore/indexer.h>
#include <musikcore/library.h>
#include <functional>
#include <memory>
#include <atomic>
#include <string>

struct LibraryIdentity;
namespace musik { namespace core { class IIndexer; } }

namespace musik { namespace core { namespace ffi {

    /* Owns production services and their wakeup pump. Neither copyable nor movable. */
    class ContextRuntime {
    public:
        ContextRuntime() = default;
        ContextRuntime(const ContextRuntime&) = delete;
        ContextRuntime(ContextRuntime&&) = delete;

        virtual ~ContextRuntime() = default;

        ContextRuntime& operator=(const ContextRuntime&) = delete;
        ContextRuntime& operator=(ContextRuntime&&) = delete;

        virtual void Start(mc_context& context, const std::string& pluginDirectory,
            mc_services services, uint32_t flags, mc_log_fn log, void* logData) = 0;
        virtual void Dispatch() = 0;
        virtual void AcknowledgeDispatch() = 0;
        virtual mc_result CreatePlayer(mc_player**) { return MC_ERROR_UNSUPPORTED; }
        virtual mc_result OpenPlayback(mc_playback** out) = 0;
        virtual mc_result SelectLibrary(mc_library_kind kind) = 0;
        virtual mc_result SelectLibrary(const std::shared_ptr<const LibraryIdentity>&) {
            return MC_ERROR_UNSUPPORTED;
        }
        virtual mc_result ReconnectRemote() = 0;
        virtual IIndexer* LocalIndexer() const = 0;
        virtual void Stop() = 0;
    };

    std::unique_ptr<ContextRuntime> make_context_runtime();
    void wake_context(mc_context& context);

    /* routes a coalesced external event to its context's dispatcher. publishers
    may outlive the context; deactivation discards queued delivery. */
    class ContextNotification {
    public:
        virtual ~ContextNotification() = default;
        virtual void Notify() = 0;
        virtual void Deactivate() = 0;
        virtual void Deliver() = 0;
    };

    mc_result create_context_notification(mc_context* context, std::function<void()> callback,
        std::shared_ptr<ContextNotification>& notification);

    struct LibraryStatus {
        mc_library_kind kind = MC_LIBRARY_NONE;
        mc_library_connection_state state = MC_LIBRARY_DISCONNECTED;
        mc_library_connection_error error = MC_LIBRARY_CONNECTION_OK;
        bool configured = false;
        std::string endpoint;
        std::string serverVersion;
    };

    std::shared_ptr<const LibraryIdentity> context_library(const mc_context& context);
    mc_result set_context_library(mc_context& context, std::shared_ptr<const LibraryIdentity> library,
        LibraryStatus status);
    void publish_indexer_event(mc_context& context, mc_indexer_event event, int updates);
    void publish_playlist_event(mc_context& context, mc_playlist_event event, int64_t playlistId);
    void publish_equalizer_event(mc_context& context);
    mc_result publish_library_selection(mc_context& context, std::shared_ptr<const LibraryIdentity> library,
        LibraryStatus status, bool selectionChanged);

    using RequestCompletion = std::function<void(mc_result, void *)>;

    struct TrackQueryLimits {
        size_t default_limit;
        size_t max_limit;
        size_t max_fields;
    };

    const TrackQueryLimits &context_track_query_limits(const mc_context &context);

    enum class RequestEffect { Read, Mutation };

    /* Work owns its inputs because cancellation can dispatch before the worker returns. */
    mc_result submit_request(mc_context *context, std::function<mc_result()> work,
        RequestCompletion completion, void *user_data, mc_request_id *out_request, std::shared_ptr<std::atomic_bool> cancel = {},
        RequestEffect effect = RequestEffect::Read, const void* owner = nullptr);

    /* Reserves a request for an external producer. Late replies may outlive the context. */
    mc_result reserve_external_request(mc_context *context, RequestCompletion completion,
        void *user_data, mc_request_id *out_request, std::function<void(mc_result)> &finish,
        RequestEffect effect = RequestEffect::Read, std::shared_ptr<std::atomic_bool> cancel = {},
        const void* owner = nullptr, std::function<void()> cancel_external = {});

    /* settle one plugin's requests on the owner thread before it is unloaded. */
    void cancel_owned_requests(mc_context& context, const void* owner) noexcept;

    /* use normal track projection with explicit plugin ownership and limits. */
    mc_result read_track_result_scoped(mc_context* context, const mc_track_result* tracks,
        const mc_track_range* range, mc_track_query_completion_fn completion, void* user_data,
        mc_request_id* request, const void* owner);

} } }

#endif
