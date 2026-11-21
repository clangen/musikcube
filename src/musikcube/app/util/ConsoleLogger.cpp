#include <stdafx.h>
#include <app/util/ConsoleLogger.h>
#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <musikcore/context.hpp>
#include <ctime>

using namespace musik::cube;
using namespace cursespp;

static std::string timestamp() {
    const auto now = std::time(nullptr);
    std::tm local {};
    char buffer[64] {};
#ifdef WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::strftime(buffer, sizeof(buffer), "%T", &local);
    return buffer;
}

ConsoleLogger::ConsoleLogger()
    : adapter(std::make_shared<SimpleScrollAdapter>()) {
    this->adapter->SetMaxEntries(1000);
    this->adapter->SetSelectable(true);
}

void MC_CALL ConsoleLogger::OnLog(void* data, mc_log_level level, mc_string_view tag, mc_string_view text) {
    static constexpr char labels[] { 'v', 'i', 'w', 'e' };
    if (level > MC_LOG_ERROR) {
        return;
    }

    const auto color = level == MC_LOG_ERROR ? Color::TextError
        : level == MC_LOG_WARNING ? Color::TextWarning : Color::Default;

    const auto tagText = std::string(musik::core::api::AsStringView(tag));
    const auto message = std::string(musik::core::api::AsStringView(text));
    auto entry = std::make_shared<SingleLineEntry>(u8fmt(
        "%s [%c] [%s] %s", timestamp().c_str(), labels[level], tagText.c_str(), message.c_str()));
    entry->SetAttrs(color);
    static_cast<ConsoleLogger*>(data)->adapter->AddEntry(entry);
}

ConsoleLogger::AdapterPtr ConsoleLogger::Adapter() const {
    return this->adapter;
}
