#ifndef MUSIKCORE_FFI_CONSUMER_MEDIA_SESSION_H
#define MUSIKCORE_FFI_CONSUMER_MEDIA_SESSION_H

#include <musikcore/media.h>
#include <musikcore/plugin/media.h>
#include <memory>

namespace musik { namespace core { namespace ffi {
    struct MediaSessionState;

    /* owns application media calls until providers can safely unload. retained
    application handles keep only stopped state after `Shutdown()` returns. */
    class MediaSession {
    public:
        MediaSession(const mc_plugin_media_service& media, const mc_plugin_audio_scratch_service& scratch);
        MediaSession(const MediaSession&) = delete;
        ~MediaSession();
        MediaSession& operator=(const MediaSession&) = delete;

        mc_result CreateStream(
            mc_string_view uri, mc_media_open_flags flags, mc_media_stream** output, mc_error** error);
        void Shutdown() noexcept;

    private:
        std::shared_ptr<MediaSessionState> state;
    };

    mc_result create_media_stream(
        mc_context* context, mc_string_view uri, mc_media_open_flags flags, mc_media_stream** output, mc_error** error);
} } }
#endif
