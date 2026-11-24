#pragma once

#include <musikcore/plugin/metadata.h>
#include <cstring>

/* writes one file's tags through the host's call-scoped metadata writer. */
class MetadataWriter {
public:
    explicit MetadataWriter(const mc_metadata_writer& writer) : writer(writer) {}

    void SetValue(const char* key, const char* value) {
        if (value && *value && status == MC_OK) {
            status = writer.set_value(writer.context, Text(key), Text(value));
        }
    }

    bool Contains(const char* key) {
        uint32_t present = 0;
        if (status == MC_OK) {
            status = writer.contains(writer.context, Text(key), &present);
        }
        return present != 0;
    }

    void SetThumbnail(const char* data, size_t length) {
        if (status == MC_OK) {
            status = writer.set_artwork(writer.context,
                { reinterpret_cast<const uint8_t*>(data), length });
        }
    }

    bool ContainsThumbnail() {
        uint32_t present = 0;
        if (status == MC_OK) {
            status = writer.contains_artwork(writer.context, &present);
        }
        return present != 0;
    }

    void SetReplayGain(const mc_replay_gain_values& gain) {
        if (status == MC_OK) {
            status = writer.set_replay_gain(writer.context, &gain);
        }
    }

    mc_result Status() const { return status; }

private:
    static mc_string_view Text(const char* value) {
        return { value, std::strlen(value) };
    }

    const mc_metadata_writer& writer;
    mc_result status { MC_OK };
};
