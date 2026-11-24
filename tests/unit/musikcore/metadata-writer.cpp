#include <musikcore/plugin/MetadataWriter.h>

#include <cstdio>
#include <new>

/* model an allocation failure while inspecting an existing thumbnail. */
class FailingTagStore final : public musik::core::sdk::ITagStore {
public:
    void Retain() override {}
    void Release() override {}
    void SetValue(const char*, const char*) override {}
    void ClearValue(const char*) override {}
    bool Contains(const char*) override { return false; }
    void SetThumbnail(const char*, long) override {}
    bool ContainsThumbnail() override { throw std::bad_alloc(); }
    void SetReplayGain(const musik::core::sdk::ReplayGain&) override {}
};

int main() {
    FailingTagStore store;
    const auto writer = musik::core::plugin::CreateMetadataWriter(store);
    uint32_t present = 0;
    const mc_string_view oversized {
        .data = reinterpret_cast<const char*>(1),
        .length = SIZE_MAX,
    };
    if (writer.contains(writer.context, oversized, &present) != MC_ERROR_INVALID_ARGUMENT) {
        std::fputs("metadata writer accepted an oversized view\n", stderr);
        return 1;
    }
    if (writer.contains_artwork(writer.context, &present) != MC_ERROR_INTERNAL) {
        std::fputs("metadata writer did not contain an allocation failure\n", stderr);
        return 1;
    }
    return 0;
}
