#include "AudioScratchBuffers.h"
#include "../common/error.h"

#include <limits>
#include <memory>
#include <vector>

/* only the core defines storage. a plugin never sees a vector, allocator, or
mutable buffer bookkeeping, and a copied view cannot alter these fields. */
struct mc_audio_scratch_buffer {
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    std::vector<float> samples;
    size_t frames = 0;
    bool writing = false;
};

namespace musik { namespace core { namespace ffi { namespace audio_scratch_buffers {
    namespace {
        bool validFormat(const mc_audio_format* format) {
            return format && format->struct_size >= sizeof(*format) && format->sample_rate && format->channels;
        }

        /* check the complete byte calculation before allocating sample storage. */
        bool validCapacity(const mc_audio_format& format, size_t frames) {
            return frames <= std::numeric_limits<size_t>::max() / format.channels / sizeof(float);
        }

        mc_result MC_CALL create(
            void*, const mc_audio_format* format, size_t frameCapacity, mc_audio_scratch_buffer** output, mc_error** error)
        {
            if (output) {
                *output = nullptr;
            }
            return exception_barrier(error, [&]() -> mc_result {
                if (!output || !validFormat(format) || !validCapacity(*format, frameCapacity)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                std::unique_ptr<mc_audio_scratch_buffer> buffer(new mc_audio_scratch_buffer());
                buffer->format = *format;
                buffer->format.struct_size = sizeof(buffer->format);
                buffer->samples.resize(frameCapacity * format->channels);
                *output = buffer.release();
                return MC_OK;
            });
        }

        mc_result MC_CALL setFormat(void*, mc_audio_scratch_buffer* buffer, const mc_audio_format* format) {
            if (!buffer || !validFormat(format)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (buffer->writing) {
                return MC_ERROR_IN_FLIGHT;
            }
            /* format is often restated for each decoded block. preserve storage
            for unchanged channel counts so reserve does not clear PCM again. */
            if (buffer->format.channels != format->channels) {
                buffer->samples.clear();
            }
            buffer->frames = 0;
            buffer->format = *format;
            buffer->format.struct_size = sizeof(buffer->format);
            return MC_OK;
        }

        mc_result MC_CALL reserve(void*, mc_audio_scratch_buffer* buffer, size_t frameCapacity) {
            return exception_barrier(nullptr, [&]() -> mc_result {
                if (!buffer || !validCapacity(buffer->format, frameCapacity)) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                if (buffer->writing) {
                    return MC_ERROR_IN_FLIGHT;
                }
                const auto samples = frameCapacity * buffer->format.channels;
                if (samples > buffer->samples.size()) {
                    buffer->samples.resize(samples);
                }
                return MC_OK;
            });
        }

        mc_result MC_CALL beginWrite(void*, mc_audio_scratch_buffer* buffer, mc_audio_buffer_view* view) {
            if (!view || view->struct_size < sizeof(*view)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            *view = MC_AUDIO_BUFFER_VIEW_DEFAULT;
            if (!buffer) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (buffer->writing) {
                return MC_ERROR_IN_FLIGHT;
            }
            buffer->writing = true;
            buffer->frames = 0;
            view->format = buffer->format;
            view->data = buffer->samples.empty() ? nullptr : buffer->samples.data();
            view->frame_capacity = buffer->samples.size() / buffer->format.channels;
            return MC_OK;
        }

        mc_result MC_CALL endWrite(void*, mc_audio_scratch_buffer* buffer, size_t writtenFrames) {
            if (!buffer || !buffer->writing) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            buffer->writing = false;
            if (writtenFrames > buffer->samples.size() / buffer->format.channels) {
                buffer->frames = 0;
                return MC_ERROR_INVALID_ARGUMENT;
            }
            buffer->frames = writtenFrames;
            return MC_OK;
        }

        void MC_CALL destroy(void*, mc_audio_scratch_buffer** buffer) {
            if (buffer) {
                delete *buffer;
                *buffer = nullptr;
            }
        }
    }

    static mc_result MC_CALL readBlock(void*, mc_audio_scratch_buffer* buffer, mc_pcm_view* view) {
        const auto status = Read(buffer, view);
        if (status == MC_OK) {
            buffer->frames = 0;
        }
        return status;
    }

    const mc_audio_scratch_buffer_api& Host() {
        static const mc_audio_scratch_buffer_api api {
            .struct_size = sizeof(mc_audio_scratch_buffer_api),
            .context = nullptr,
            .create = create,
            .set_format = setFormat,
            .reserve = reserve,
            .begin_write = beginWrite,
            .end_write = endWrite,
            .destroy = destroy,
            .read_block = readBlock,
        };
        return api;
    }

    mc_result Read(const mc_audio_scratch_buffer* buffer, mc_pcm_view* view) {
        if (!view || view->struct_size < sizeof(*view)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        *view = MC_PCM_VIEW_DEFAULT;
        if (!buffer) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (buffer->writing) {
            return MC_ERROR_IN_FLIGHT;
        }
        view->data = buffer->samples.empty() ? nullptr : buffer->samples.data();
        view->format = buffer->format;
        view->frame_count = buffer->frames;
        return MC_OK;
    }

} } } }
