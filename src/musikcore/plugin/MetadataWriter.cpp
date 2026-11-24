#include "MetadataWriter.h"

#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/common/primitives.h>
#include <climits>
#include <string>

namespace musik { namespace core { namespace plugin {

    namespace {
        mc_result MC_CALL setValue(void* context, mc_string_view key, mc_string_view value) {
            if (!ffi::valid_text(key, false) || !ffi::valid_text(value, false)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                const std::string keyText(key.data, key.length);
                const std::string valueText(value.data, value.length);
                static_cast<sdk::ITagStore*>(context)->SetValue(keyText.c_str(), valueText.c_str());
                return MC_OK;
            });
        }

        mc_result MC_CALL clearValue(void* context, mc_string_view key) {
            if (!ffi::valid_text(key, false)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                const std::string text(key.data, key.length);
                static_cast<sdk::ITagStore*>(context)->ClearValue(text.c_str());
                return MC_OK;
            });
        }

        mc_result MC_CALL containsValue(void* context, mc_string_view key, uint32_t* present) {
            if (!ffi::valid_text(key, false) || !present) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                const std::string text(key.data, key.length);
                *present = static_cast<sdk::ITagStore*>(context)->Contains(text.c_str()) ? 1 : 0;
                return MC_OK;
            });
        }

        mc_result MC_CALL setArtwork(void* context, mc_byte_view bytes) {
            if (!bytes.data || !bytes.length || bytes.length > static_cast<size_t>(INT_MAX)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                static_cast<sdk::ITagStore*>(context)->SetThumbnail(reinterpret_cast<const char*>(bytes.data),
                    static_cast<long>(bytes.length));
                return MC_OK;
            });
        }

        mc_result MC_CALL containsArtwork(void* context, uint32_t* present) {
            if (!present) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                *present = static_cast<sdk::ITagStore*>(context)->ContainsThumbnail() ? 1 : 0;
                return MC_OK;
            });
        }

        mc_result MC_CALL setReplayGain(void* context, const mc_replay_gain_values* gain) {
            if (!gain || gain->struct_size < sizeof(*gain)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            return ffi::exception_barrier(nullptr, [&] {
                const sdk::ReplayGain values {
                    .trackGain = gain->track_gain,
                    .trackPeak = gain->track_peak,
                    .albumGain = gain->album_gain,
                    .albumPeak = gain->album_peak,
                };
                static_cast<sdk::ITagStore*>(context)->SetReplayGain(values);
                return MC_OK;
            });
        }

    }

    mc_metadata_writer CreateMetadataWriter(sdk::ITagStore& store) {
        return {
            .struct_size = sizeof(mc_metadata_writer),
            .context = &store,
            .set_value = setValue,
            .clear = clearValue,
            .contains = containsValue,
            .set_artwork = setArtwork,
            .contains_artwork = containsArtwork,
            .set_replay_gain = setReplayGain,
        };
    }

} } }
