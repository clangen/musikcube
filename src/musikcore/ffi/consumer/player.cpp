#include "player-handle.h"
#include "../common/error.h"
#include "../common/primitives.h"
#include <cmath>

using musik::core::ffi::exception_barrier;

mc_result MC_CALL mc_player_play_uri(mc_player* player, mc_string_view uri) {
    if (!player || !musik::core::ffi::valid_text(uri, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&] {
        return player->Play(uri);
    });
}

/* validate shared handle state before the transport sees a command. */
static mc_result control(mc_player* player, PlayerCommand command, double value = 0) {
    if (!player) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&] {
        return player->Control(command, value);
    });
}
mc_result MC_CALL mc_player_pause(mc_player* player) {
    return control(player, PlayerCommand::Pause);
}
mc_result MC_CALL mc_player_resume(mc_player* player) {
    return control(player, PlayerCommand::Resume);
}
mc_result MC_CALL mc_player_stop(mc_player* player) {
    return control(player, PlayerCommand::Stop);
}
mc_result MC_CALL mc_player_seek(mc_player* player, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return control(player, PlayerCommand::Seek, seconds);
}
mc_result MC_CALL mc_player_set_volume(mc_player* player, double volume) {
    if (!std::isfinite(volume) || volume < 0 || volume > 1) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return control(player, PlayerCommand::Volume, volume);
}
mc_result MC_CALL mc_player_get_snapshot(const mc_player* player, mc_player_snapshot* snapshot) {
    if (!player || !snapshot || snapshot->struct_size < sizeof(*snapshot)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&] {
        mc_player_snapshot result = MC_PLAYER_SNAPSHOT_DEFAULT;
        const auto status = player->Snapshot(result);
        if (status == MC_OK) {
            result.struct_size = snapshot->struct_size;
            *snapshot = result;
        }
        return status;
    });
}
mc_result MC_CALL mc_player_set_callback(mc_player* player, mc_player_changed_fn callback, void* data) {
    if (!player) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&] {
        return player->SetCallback(callback, data);
    });
}
mc_result MC_CALL mc_player_destroy(mc_player** player) {
    if (!player || !*player) {
        return MC_OK;
    }
    return exception_barrier(nullptr, [&] {
        const auto status = (*player)->Close();
        if (status == MC_OK) {
            delete *player;
            *player = nullptr;
        }
        return status;
    });
}
