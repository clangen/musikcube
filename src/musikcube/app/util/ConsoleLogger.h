#pragma once

/**
 * @file ConsoleLogger.h
 * @brief Debug logging backend that captures log output for the console.
 * @details Implements musik::debug::IBackend, formats each log entry with its
 *          tag and level, assigns a color, and appends it to a
 *          SimpleScrollAdapter that the console layout renders. Updates are
 *          marshalled to the UI thread through a message queue.
 */

#include <musikcore/debug.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <cursespp/Colors.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <sigslot/sigslot.h>

namespace musik { namespace cube {

    /**
     * @brief In-memory log capture backend.
     * @details Receives verbose, info, warning and error log messages, formats
     *          them and appends them to a scroll adapter. Emits a signal when
     *          new entries arrive so the console view can refresh.
     */
    class ConsoleLogger:
        public musik::debug::IBackend,
        public musik::core::runtime::IMessageTarget,
        public sigslot::has_slots<>
    {
        public:
            using AdapterPtr = std::shared_ptr<cursespp::SimpleScrollAdapter>;

            /**
             * @brief Creates a logger that dispatches UI updates through the
             *        given message queue.
             * @param messageQueue the queue used to post refresh messages
             */
            ConsoleLogger(musik::core::runtime::IMessageQueue& messageQueue);
            ConsoleLogger(const ConsoleLogger& other) = delete;
            ConsoleLogger& operator=(const ConsoleLogger& other) = delete;
            /**
             * @brief Destroys the logger and unregisters the backend.
             */
            virtual ~ConsoleLogger();

            /**
             * @brief Records a verbose log message.
             * @param tag the message source tag
             * @param string the log text
             */
            virtual void verbose(const std::string& tag, const std::string& string) override;
            /**
             * @brief Records an informational log message.
             * @param tag the message source tag
             * @param string the log text
             */
            virtual void info(const std::string& tag, const std::string& string) override;
            /**
             * @brief Records a warning log message.
             * @param tag the message source tag
             * @param string the log text
             */
            virtual void warning(const std::string& tag, const std::string& string) override;
            /**
             * @brief Records an error log message.
             * @param tag the message source tag
             * @param string the log text
             */
            virtual void error(const std::string& tag, const std::string& string) override;

            /**
             * @brief Handles the refresh message posted by the backend.
             * @param message the message to process
             */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /**
             * @brief Returns the adapter that holds the log entries.
             * @return the shared scroll adapter
             */
            AdapterPtr Adapter();

        private:
            /**
             * @brief Formats a log entry and appends it to the adapter.
             * @param tag the message source tag
             * @param level the log level name
             * @param str the log text
             * @param color the color used to render the entry
             */
            void FormatAndDispatch(
                const std::string& tag,
                const std::string& level,
                const std::string& str,
                cursespp::Color color);

            AdapterPtr adapter;                                  /**< the adapter holding the log entries */
            musik::core::runtime::IMessageQueue& messageQueue;   /**< queue used to post UI refresh messages */
    };

} }