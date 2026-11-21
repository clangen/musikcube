#pragma once

#include <musikcore/log.h>
#include <cursespp/SimpleScrollAdapter.h>

namespace musik { namespace cube {

    /* Owns the console's log rows. Neither copyable nor movable. */
    class ConsoleLogger {
    public:
        using AdapterPtr = std::shared_ptr<cursespp::SimpleScrollAdapter>;

        ConsoleLogger();
        ConsoleLogger(const ConsoleLogger&) = delete;
        ConsoleLogger(ConsoleLogger&&) = delete;

        ConsoleLogger& operator=(const ConsoleLogger&) = delete;
        ConsoleLogger& operator=(ConsoleLogger&&) = delete;

        static void MC_CALL OnLog(void* data, mc_log_level level, mc_string_view tag, mc_string_view text);
        AdapterPtr Adapter() const;

    private:
        AdapterPtr adapter;
    };

} }
