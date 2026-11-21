#pragma once
#include <musikcore/player.h>
#include <memory>

enum class PlayerCommand {
    Pause,
    Resume,
    Stop,
    Seek,
    Volume
};

/* private polymorphism stays behind the C handle, including in ABI-only builds. */
struct mc_player {
    virtual ~mc_player() = default;
    virtual mc_result Play(mc_string_view uri) = 0;
    virtual mc_result Control(PlayerCommand command, double value) = 0;
    virtual mc_result Snapshot(mc_player_snapshot& snapshot) const = 0;
    virtual mc_result SetCallback(mc_player_changed_fn callback, void* data) = 0;
    virtual mc_result Close() = 0;
};

namespace musik { namespace core { namespace runtime {
    class MessageQueue;
} } }
namespace musik { namespace core { namespace ffi {
    /* held weakly by the runtime; handles can safely outlive its shutdown. */
    class UriPlayerSession {
        public:
            virtual ~UriPlayerSession() = default;
            virtual void Shutdown() = 0;
    };
    std::shared_ptr<UriPlayerSession> CreateUriPlayer(std::shared_ptr<runtime::MessageQueue> queue, mc_player** out);
} } }
