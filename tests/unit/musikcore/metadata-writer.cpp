#include <musikcore/plugin/MetadataWriter.h>
#include <musikcore/library/track/Track.h>

#include <cstdio>
#include <new>

/* model an allocation failure while inspecting an existing thumbnail. */
class FailingTrack final : public musik::core::Track {
public:
    void SetValue(const char*, const char*) override {}
    void ClearValue(const char*) override {}
    bool Contains(const char*) override { return false; }
    void SetThumbnail(const char*, long) override {}
    bool ContainsThumbnail() override { throw std::bad_alloc(); }
    void SetReplayGain(const mc_replay_gain_values& value) override { gain = value; ++gainWrites; }
    int64_t GetId() override { return 0; }
    mc_replay_gain_values GetReplayGain() override { return gain; }
    musik::core::MetadataState GetMetadataState() override {
        return musik::core::MetadataState::NotLoaded;
    }
    void SetId(int64_t) override {}
    std::string GetString(const char*) override { return {}; }
    std::string Uri() override { return {}; }
    MetadataIteratorRange GetValues(const char*) override { return { values.begin(), values.end() }; }
    MetadataIteratorRange GetAllValues() override { return { values.begin(), values.end() }; }
    musik::core::TrackPtr Copy() override { return {}; }
    void SetMetadataState(musik::core::MetadataState) override {}
    mc_replay_gain_values gain = MC_REPLAY_GAIN_VALUES_DEFAULT;
    int gainWrites = 0;
private:
    MetadataMap values;
};

int main() {
    FailingTrack store;
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
    mc_replay_gain_values gain = MC_REPLAY_GAIN_VALUES_DEFAULT;
    gain.track_gain = -6.0f;
    gain.album_peak = 0.8f;
    if (writer.set_replay_gain(writer.context, &gain) != MC_OK ||
        store.gainWrites != 1 || store.gain.struct_size != sizeof(gain) ||
        store.gain.track_gain != -6.0f || store.gain.album_peak != 0.8f)
    {
        std::fputs("metadata writer did not preserve replay gain values\n", stderr);
        return 1;
    }
    gain.struct_size = 0;
    if (writer.set_replay_gain(writer.context, &gain) != MC_ERROR_INVALID_ARGUMENT || store.gainWrites != 1) {
        std::fputs("metadata writer accepted an invalid replay gain value\n", stderr);
        return 1;
    }
    return 0;
}
