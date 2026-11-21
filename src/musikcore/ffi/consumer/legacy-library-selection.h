#pragma once

#include <musikcore/context.hpp>
#include <musikcore/library/ILibrary.h>

namespace musik { namespace core { namespace audio { class PlaybackService; } } }

namespace musik { namespace core { namespace ffi {
    /* Supply the ILibrary interface while playback and plugins still consume it. */
    ILibraryPtr CreateLegacyLibrary();
    void AttachLegacyLibrary(mc_context& context, const ILibraryPtr& library, audio::PlaybackService& playback);
    void StartLegacyApp(api::Context& musikcore, runtime::IMessageQueue& queue);
    void StopLegacyIndexing();
    void ShutdownLegacyLibraries();
} } }
