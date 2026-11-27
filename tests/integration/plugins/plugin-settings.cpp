#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Settings.h>
#include <musikcore/support/Common.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cstdio>
#include <cstdlib>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "plugin settings: %s\n", message);
        std::abort();
    }
}

int main() {
    auto settings = musik::core::plugin::SettingsService("settings writer test");
    auto other = musik::core::plugin::SettingsService("other settings test");
    const mc_setting_entry entries[] {
        { .key = { "locked", 6 }, .value = { .type = MC_SETTING_BOOLEAN, .data = { .boolean = true, }, }, },
        { .key = { "duration", 8 }, .value = { .type = MC_SETTING_NUMBER, .data = { .number = 73.5, }, }, },
        { .key = { "preset", 6 }, .value = { .type = MC_SETTING_TEXT, .data = { .text = { "folder/last.milk", 16 }, }, }, },
    };
    require(settings.write_values(settings.context, entries, std::size(entries)) == MC_OK, "save batch");
    bool locked = false;
    require(settings.read_boolean(settings.context, { "locked", 6 }, false, &locked) == MC_OK && locked,
        "read saved value");
    require(other.read_boolean(other.context, { "locked", 6 }, false, &locked) == MC_OK && !locked,
        "writes stay scoped to their own plugin");
    const auto file = std::filesystem::path(musik::core::GetDataDirectory()) / "plugin_settingswritertest.json";
    std::ifstream input(file);
    nlohmann::json saved;
    input >> saved;
    require(saved["locked"] == true && saved["duration"] == 73.5 && saved["preset"] == "folder/last.milk",
        "batch persisted through real store");
    auto invalid = entries[1];
    invalid.value.data.number = std::numeric_limits<double>::quiet_NaN();
    auto changed = entries[0];
    changed.value.data.boolean = false;
    const mc_setting_entry batch[] { changed, invalid };
    require(settings.write_values(settings.context, batch, 2) == MC_ERROR_INVALID_ARGUMENT,
        "reject entire invalid batch before mutation");
    require(settings.read_boolean(settings.context, { "locked", 6 }, false, &locked) == MC_OK && locked,
        "valid prefix of rejected batch did not apply");
    require(settings.write_values(settings.context, nullptr, 1) == MC_ERROR_INVALID_ARGUMENT, "reject missing entries");
    require(settings.write_values(settings.context, nullptr, 0) == MC_OK, "empty update succeeds");
    std::filesystem::remove(file);
    std::filesystem::create_directory(file);
    require(settings.write_values(settings.context, &changed, 1) == MC_ERROR_IO, "report persistence failure");
    require(settings.read_boolean(settings.context, { "locked", 6 }, false, &locked) == MC_OK && locked,
        "failed persistence did not mutate memory");
    std::filesystem::remove(file);
}
