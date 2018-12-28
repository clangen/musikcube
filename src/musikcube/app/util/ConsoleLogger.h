#pragma once

#include <core/debug.h>
#include <core/runtime/IMessageQueue.h>
#include <cursespp/Colors.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <sigslot/sigslot.h>

namespace musik { namespace cube {

    class ConsoleLogger:
        public musik::debug::IBackend,
        public musik::core::runtime::IMessageTarget,
        public sigslot::has_slots<>
    {
        public:
            using AdapterPtr = std::shared_ptr<cursespp::SimpleScrollAdapter>;

            ConsoleLogger(musik::core::runtime::IMessageQueue& messageQueue);
            ConsoleLogger(const ConsoleLogger& other) = delete;
            ConsoleLogger& operator=(const ConsoleLogger& other) = delete;

            virtual void verbose(const std::string& tag, const std::string& string) override;
            virtual void info(const std::string& tag, const std::string& string) override;
            virtual void warning(const std::string& tag, const std::string& string) override;
            virtual void error(const std::string& tag, const std::string& string) override;

            virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            AdapterPtr Adapter();

        private:
            void FormatAndDispatch(
                const std::string& tag,
                const std::string& level,
                const std::string& str,
                cursespp::Color color);

            AdapterPtr adapter;
            musik::core::runtime::IMessageQueue& messageQueue;
    };

} }