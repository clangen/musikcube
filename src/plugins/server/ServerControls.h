#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <string_view>
#include <musikcore/abi.h>

struct Context;
struct mc_playback;

/* handles server control requests using the engine's C APIs. */
bool ExecuteServerControl(mc_playback* playback, std::string_view operation,
    const nlohmann::json& options, nlohmann::json& output);

/* schedule a scan without waiting on the server's network thread. */
mc_result ScheduleServerIndexer(Context& context, const nlohmann::json& options,
    std::function<void(mc_result)> completion);
