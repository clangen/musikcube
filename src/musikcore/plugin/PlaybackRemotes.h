#pragma once

#include <common/DeleteDefaults.h>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>
#include <musikcore/abi.h>

namespace musik { namespace core { namespace audio { class PlaybackService; } } }
namespace musik { namespace core { namespace runtime { class IMessageQueue; } } }
namespace musik { namespace core { class LibraryBackend; } }
struct mc_context;

namespace musik { namespace core { namespace plugin {

    /* owns the C playback remotes for one managed runtime session. */
    class PlaybackRemotes {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(PlaybackRemotes);

        PlaybackRemotes(mc_context& context, std::shared_ptr<LibraryBackend> local,
            audio::PlaybackService& playback, runtime::IMessageQueue& queue);
        ~PlaybackRemotes();

        void Stop();
        mc_result Reload(std::string_view pluginId);

    private:
        class Remote;

        mc_result ValidateCallingThread() const;

        mc_context& context;
        std::shared_ptr<LibraryBackend> local;
        audio::PlaybackService& playback;
        runtime::IMessageQueue& queue;
        std::thread::id owner;
        std::vector<std::unique_ptr<Remote>> remotes;
    };

} } }
