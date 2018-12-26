#include <stdafx.h>
#include <app/util/ConsoleLogger.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/SingleLineEntry.h>
#include <core/runtime/Message.h>
#include <app/util/Messages.h>
#include <time.h>

using namespace musik;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

static std::string timestamp() {
    time_t rawtime = { 0 };
    char buffer[64] = { 0 };
    time(&rawtime);
#ifdef WIN32
    struct tm timeinfo = { 0 };
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, sizeof(buffer), "%T", &timeinfo);
#else
    struct tm* timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%T", timeinfo);
#endif
    return std::string(buffer);
}

struct LogMessage: public Message {
    std::string logValue;
    Color color;

    LogMessage(IMessageTarget* target, const std::string& value, const Color color)
    : Message(target, message::DebugLog, 0, 0) {
        this->logValue = value;
        this->color = color;
    }
};

ConsoleLogger::ConsoleLogger(IMessageQueue& messageQueue)
: messageQueue(messageQueue) {
    this->adapter = std::make_shared<SimpleScrollAdapter>();
    this->adapter->SetSelectable(true);
}

void ConsoleLogger::verbose(const std::string& tag, const std::string& string) {
    this->FormatAndDispatch(tag, "v", string, Color::Default);
}

void ConsoleLogger::info(const std::string& tag, const std::string& string) {
    this->FormatAndDispatch(tag, "i", string, Color::Default);
}

void ConsoleLogger::warning(const std::string& tag, const std::string& string) {
    this->FormatAndDispatch(tag, "w", string, Color::TextWarning);
}

void ConsoleLogger::error(const std::string& tag, const std::string& string) {
    this->FormatAndDispatch(tag, "e", string, Color::TextError);
}

void ConsoleLogger::FormatAndDispatch(
    const std::string& tag, const std::string& level, const std::string& str, Color color)
{
    const std::string formatted = u8fmt(
        "%s [%s] [%s] %s", timestamp().c_str(), level.c_str(), tag.c_str(), str.c_str());

    this->messageQueue.Post(std::make_shared<LogMessage>(this, formatted, color));
}

void ConsoleLogger::ProcessMessage(IMessage& message) {
    if (message.Type() == message::DebugLog) {
        auto log = static_cast<LogMessage*>(&message);
        auto entry = std::make_shared<SingleLineEntry>(log->logValue);
        entry->SetAttrs(log->color);
        this->adapter->AddEntry(entry);
    }
}

ConsoleLogger::AdapterPtr ConsoleLogger::Adapter() {
    return this->adapter;
}
