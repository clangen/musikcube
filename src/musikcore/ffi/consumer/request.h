#ifndef MUSIKCORE_FFI_CONSUMER_REQUEST_H
#define MUSIKCORE_FFI_CONSUMER_REQUEST_H

#include <musikcore/context.h>
#include <musikcore/indexer.h>
#include <musikcore/library.h>
#include <functional>
#include <memory>
#include <string>

struct LibraryIdentity;
namespace musik { namespace core { namespace audio { class PlaybackService; } } }
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

        virtual void Start(mc_context& context) = 0;
        virtual void Dispatch() = 0;
        virtual void AcknowledgeDispatch() = 0;
        virtual void Stop() = 0;
    };

    std::unique_ptr<ContextRuntime> make_context_runtime();
    void wake_context(mc_context& context);

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
    void attach_library_selector(mc_context& context, std::function<mc_result(mc_library_kind)> selector,
        std::function<void()> reconnect, IIndexer& indexer);
    void publish_indexer_event(mc_context& context, mc_indexer_event event, int updates);
    void publish_playlist_event(mc_context& context, mc_playlist_event event, int64_t playlistId);
    void publish_equalizer_event(mc_context& context);
    mc_result publish_library_selection(mc_context& context, std::shared_ptr<const LibraryIdentity> library,
        LibraryStatus status, bool selectionChanged);
    using OpenPlayback = mc_result (*)(audio::PlaybackService&, mc_playback**);
    mc_result attach_context_playback(mc_context& context, audio::PlaybackService& playback, OpenPlayback open);
    void own_context_playback(mc_context& context, std::shared_ptr<audio::PlaybackService> playback);

    using RequestCompletion = std::function<void(mc_result, void *)>;

    struct TrackQueryLimits {
        size_t default_limit;
        size_t max_limit;
        size_t max_fields;
    };

    const TrackQueryLimits &context_track_query_limits(const mc_context &context);

    /* Work owns its inputs because cancellation can dispatch before the worker returns. */
    mc_result submit_request(mc_context *context, std::function<mc_result()> work,
        RequestCompletion completion, void *user_data, mc_request_id *out_request);

    /* Reserves a request for an external producer. Late replies may outlive the context. */
    mc_result reserve_external_request(mc_context *context, RequestCompletion completion,
        void *user_data, mc_request_id *out_request, std::function<void(mc_result)> &finish);

} } }

#endif
