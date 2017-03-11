
/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <pulse/xmalloc.h>

#include "pulse_blocking_stream.h"

struct pa_blocking {
    pa_threaded_mainloop *mainloop;
    pa_context *context;
    pa_stream *stream;
    pa_stream_direction_t direction;

    const void *read_data;
    size_t read_index, read_length;
    size_t channels;
    int sink_id;
    int hw_volume;

    int operation_success;
};

#define CHECK_VALIDITY_RETURN_ANY(rerror, expression, error, ret)       \
    do {                                                                \
        if (!(expression)) {                                            \
            if (rerror)                                                 \
                *(rerror) = error;                                      \
            return (ret);                                               \
        }                                                               \
    } while(false);

#define CHECK_SUCCESS_GOTO(p, rerror, expression, label)        \
    do {                                                        \
        if (!(expression)) {                                    \
            if (rerror)                                         \
                *(rerror) = pa_context_errno((p)->context);     \
            goto label;                                         \
        }                                                       \
    } while(false);

#define CHECK_DEAD_GOTO(p, rerror, label)                               \
    do {                                                                \
        if (!(p)->context || !PA_CONTEXT_IS_GOOD(pa_context_get_state((p)->context)) || \
            !(p)->stream || !PA_STREAM_IS_GOOD(pa_stream_get_state((p)->stream))) { \
            if (((p)->context && pa_context_get_state((p)->context) == PA_CONTEXT_FAILED) || \
                ((p)->stream && pa_stream_get_state((p)->stream) == PA_STREAM_FAILED)) { \
                if (rerror)                                             \
                    *(rerror) = pa_context_errno((p)->context);         \
            } else                                                      \
                if (rerror)                                             \
                    *(rerror) = PA_ERR_BADSTATE;                        \
            goto label;                                                 \
        }                                                               \
    } while(false);

static void context_state_cb(pa_context *c, void *userdata) {
    pa_blocking *p = userdata;
    assert(c);
    assert(p);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(p->mainloop, 0);
            break;

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
    }
}

static void success_context_cb(pa_context *c, int success, void *userdata) {
    pa_blocking *p = userdata;
    assert(c);
    assert(p);

    p->operation_success = success;
    pa_threaded_mainloop_signal(p->mainloop, 0);
}

static void stream_state_cb(pa_stream *s, void * userdata) {
    pa_blocking *p = userdata;
    assert(s);
    assert(p);

    switch (pa_stream_get_state(s)) {

        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(p->mainloop, 0);
            break;

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
    pa_blocking *p = userdata;
    assert(p);

    pa_threaded_mainloop_signal(p->mainloop, 0);
}

static void stream_latency_update_cb(pa_stream *s, void *userdata) {
    pa_blocking *p = userdata;

    assert(p);

    pa_threaded_mainloop_signal(p->mainloop, 0);
}

pa_blocking* pa_blocking_new(
        const char *server,
        const char *name,
        pa_stream_direction_t dir,
        const char *dev,
        const char *stream_name,
        const pa_sample_spec *ss,
        const pa_channel_map *map,
        const pa_buffer_attr *attr,
        int *rerror) {

    pa_blocking *p;
    int error = PA_ERR_INTERNAL, r;

    CHECK_VALIDITY_RETURN_ANY(rerror, !server || *server, PA_ERR_INVALID, NULL);
    CHECK_VALIDITY_RETURN_ANY(rerror, dir == PA_STREAM_PLAYBACK || dir == PA_STREAM_RECORD, PA_ERR_INVALID, NULL);
    CHECK_VALIDITY_RETURN_ANY(rerror, !dev || *dev, PA_ERR_INVALID, NULL);
    CHECK_VALIDITY_RETURN_ANY(rerror, ss && pa_sample_spec_valid(ss), PA_ERR_INVALID, NULL);
    CHECK_VALIDITY_RETURN_ANY(rerror, !map || (pa_channel_map_valid(map) && map->channels == ss->channels), PA_ERR_INVALID, NULL)

    p = pa_xnew0(pa_blocking, 1);
    p->direction = dir;
    p->channels = ss->channels;
    p->sink_id = -1;
    p->hw_volume = -1;

    if (!(p->mainloop = pa_threaded_mainloop_new()))
        goto fail;

    if (!(p->context = pa_context_new(pa_threaded_mainloop_get_api(p->mainloop), name)))
        goto fail;

    pa_context_set_state_callback(p->context, context_state_cb, p);

    if (pa_context_connect(p->context, server, 0, NULL) < 0) {
        error = pa_context_errno(p->context);
        goto fail;
    }

    pa_threaded_mainloop_lock(p->mainloop);

    if (pa_threaded_mainloop_start(p->mainloop) < 0)
        goto unlock_and_fail;

    for (;;) {
        pa_context_state_t state;

        state = pa_context_get_state(p->context);

        if (state == PA_CONTEXT_READY)
            break;

        if (!PA_CONTEXT_IS_GOOD(state)) {
            error = pa_context_errno(p->context);
            goto unlock_and_fail;
        }

        /* Wait until the context is ready */
        pa_threaded_mainloop_wait(p->mainloop);
    }

    if (!(p->stream = pa_stream_new(p->context, stream_name, ss, map))) {
        error = pa_context_errno(p->context);
        goto unlock_and_fail;
    }

    pa_stream_set_state_callback(p->stream, stream_state_cb, p);
    pa_stream_set_read_callback(p->stream, stream_request_cb, p);
    pa_stream_set_write_callback(p->stream, stream_request_cb, p);
    pa_stream_set_latency_update_callback(p->stream, stream_latency_update_cb, p);

    if (dir == PA_STREAM_PLAYBACK)
        r = pa_stream_connect_playback(p->stream, dev, attr,
                                       PA_STREAM_INTERPOLATE_TIMING
                                       |PA_STREAM_ADJUST_LATENCY
                                       |PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);
    else
        r = pa_stream_connect_record(p->stream, dev, attr,
                                     PA_STREAM_INTERPOLATE_TIMING
                                     |PA_STREAM_ADJUST_LATENCY
                                     |PA_STREAM_AUTO_TIMING_UPDATE);

    if (r < 0) {
        error = pa_context_errno(p->context);
        goto unlock_and_fail;
    }

    for (;;) {
        pa_stream_state_t state;

        state = pa_stream_get_state(p->stream);

        if (state == PA_STREAM_READY)
            break;

        if (!PA_STREAM_IS_GOOD(state)) {
            error = pa_context_errno(p->context);
            goto unlock_and_fail;
        }

        /* Wait until the stream is ready */
        pa_threaded_mainloop_wait(p->mainloop);
    }

    pa_threaded_mainloop_unlock(p->mainloop);

    return p;

unlock_and_fail:
    pa_threaded_mainloop_unlock(p->mainloop);

fail:
    if (rerror)
        *rerror = error;
    pa_blocking_free(p);
    return NULL;
}

void pa_blocking_free(pa_blocking *s) {
    assert(s);

    if (s->mainloop)
        pa_threaded_mainloop_stop(s->mainloop);

    if (s->stream)
        pa_stream_unref(s->stream);

    if (s->context) {
        pa_context_disconnect(s->context);
        pa_context_unref(s->context);
    }

    if (s->mainloop)
        pa_threaded_mainloop_free(s->mainloop);

    pa_xfree(s);
}

int pa_blocking_write(pa_blocking *p, const void*data, size_t length, int *rerror) {
    assert(p);

    CHECK_VALIDITY_RETURN_ANY(rerror, p->direction == PA_STREAM_PLAYBACK, PA_ERR_BADSTATE, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, data, PA_ERR_INVALID, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, length > 0, PA_ERR_INVALID, -1);

    pa_threaded_mainloop_lock(p->mainloop);

    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    while (length > 0) {
        size_t l;
        int r;

        while (!(l = pa_stream_writable_size(p->stream))) {
            pa_threaded_mainloop_wait(p->mainloop);
            CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
        }

        CHECK_SUCCESS_GOTO(p, rerror, l != (size_t) -1, unlock_and_fail);

        if (l > length)
            l = length;

        r = pa_stream_write(p->stream, data, l, NULL, 0LL, PA_SEEK_RELATIVE);
        CHECK_SUCCESS_GOTO(p, rerror, r >= 0, unlock_and_fail);

        data = (const uint8_t*) data + l;
        length -= l;
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return 0;

unlock_and_fail:
    pa_threaded_mainloop_unlock(p->mainloop);
    return -1;
}

int pa_blocking_read(pa_blocking *p, void*data, size_t length, int *rerror) {
    assert(p);

    CHECK_VALIDITY_RETURN_ANY(rerror, p->direction == PA_STREAM_RECORD, PA_ERR_BADSTATE, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, data, PA_ERR_INVALID, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, length > 0, PA_ERR_INVALID, -1);

    pa_threaded_mainloop_lock(p->mainloop);

    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    while (length > 0) {
        size_t l;

        while (!p->read_data) {
            int r;

            r = pa_stream_peek(p->stream, &p->read_data, &p->read_length);
            CHECK_SUCCESS_GOTO(p, rerror, r == 0, unlock_and_fail);

            if (p->read_length <= 0) {
                pa_threaded_mainloop_wait(p->mainloop);
                CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
            } else if (!p->read_data) {
                /* There's a hole in the stream, skip it. We could generate
                 * silence, but that wouldn't work for compressed streams. */
                r = pa_stream_drop(p->stream);
                CHECK_SUCCESS_GOTO(p, rerror, r == 0, unlock_and_fail);
            } else
                p->read_index = 0;
        }

        l = p->read_length < length ? p->read_length : length;
        memcpy(data, (const uint8_t*) p->read_data+p->read_index, l);

        data = (uint8_t*) data + l;
        length -= l;

        p->read_index += l;
        p->read_length -= l;

        if (!p->read_length) {
            int r;

            r = pa_stream_drop(p->stream);
            p->read_data = NULL;
            p->read_length = 0;
            p->read_index = 0;

            CHECK_SUCCESS_GOTO(p, rerror, r == 0, unlock_and_fail);
        }
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return 0;

unlock_and_fail:
    pa_threaded_mainloop_unlock(p->mainloop);
    return -1;
}

static void success_cb(pa_stream *s, int success, void *userdata) {
    pa_blocking *p = userdata;

    assert(s);
    assert(p);

    p->operation_success = success;
    pa_threaded_mainloop_signal(p->mainloop, 0);
}

int pa_blocking_drain(pa_blocking *p, int *rerror) {
    pa_operation *o = NULL;

    assert(p);

    CHECK_VALIDITY_RETURN_ANY(rerror, p->direction == PA_STREAM_PLAYBACK, PA_ERR_BADSTATE, -1);

    pa_threaded_mainloop_lock(p->mainloop);
    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    o = pa_stream_drain(p->stream, success_cb, p);
    CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);

    p->operation_success = 0;
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(p->mainloop);
        CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
    }
    CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(p->mainloop);

    return 0;

unlock_and_fail:

    if (o) {
        pa_operation_cancel(o);
        pa_operation_unref(o);
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return -1;
}

int pa_blocking_flush(pa_blocking *p, int *rerror) {
    pa_operation *o = NULL;

    assert(p);

    pa_threaded_mainloop_lock(p->mainloop);
    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    o = pa_stream_flush(p->stream, success_cb, p);
    CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);

    p->operation_success = 0;
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(p->mainloop);
        CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
    }
    CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(p->mainloop);

    return 0;

unlock_and_fail:

    if (o) {
        pa_operation_cancel(o);
        pa_operation_unref(o);
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return -1;
}

pa_usec_t pa_blocking_get_latency(pa_blocking *p, int *rerror) {
    pa_usec_t t;

    assert(p);

    pa_threaded_mainloop_lock(p->mainloop);

    for (;;) {
        int negative;

        CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

        if (pa_stream_get_latency(p->stream, &t, &negative) >= 0) {
            pa_usec_t extra = 0;

            if (p->direction == PA_STREAM_RECORD)
                extra = pa_bytes_to_usec(p->read_length, pa_stream_get_sample_spec(p->stream));

            if (negative) {
                if (extra > t)
                    t = extra - t;
                else
                    t = 0;
            } else
                t += extra;

            break;
        }

        CHECK_SUCCESS_GOTO(p, rerror, pa_context_errno(p->context) == PA_ERR_NODATA, unlock_and_fail);

        /* Wait until latency data is available again */
        pa_threaded_mainloop_wait(p->mainloop);
    }

    pa_threaded_mainloop_unlock(p->mainloop);

    return t;

unlock_and_fail:

    pa_threaded_mainloop_unlock(p->mainloop);
    return (pa_usec_t) -1;
}

int pa_blocking_set_volume(pa_blocking *p, int volume, int *rerror) {
    pa_operation *o = NULL;
    pa_stream *s = NULL;
    uint32_t idx;
    pa_cvolume cv;
    pa_volume_t v;

    assert(p);

    CHECK_VALIDITY_RETURN_ANY(rerror, p->direction == PA_STREAM_PLAYBACK, PA_ERR_BADSTATE, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, volume >= 0, PA_ERR_INVALID, -1);
    CHECK_VALIDITY_RETURN_ANY(rerror, volume <= 65536, PA_ERR_INVALID, -1);

    pa_threaded_mainloop_lock(p->mainloop);
    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    CHECK_SUCCESS_GOTO(p, rerror, ((idx = pa_stream_get_index (p->stream)) != PA_INVALID_INDEX), unlock_and_fail);

    s = p->stream;
    assert(s);
    pa_cvolume_set(&cv, p->channels, volume);

    o = pa_context_set_sink_input_volume (p->context, idx, &cv, success_context_cb, p);
    CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);

    p->operation_success = 0;
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(p->mainloop);
        CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
    }
    CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(p->mainloop);

    return 0;

unlock_and_fail:

    if (o) {
        pa_operation_cancel(o);
        pa_operation_unref(o);
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return -1;
}

static void sink_info_cb(pa_context* c, const pa_sink_info* info, int eol, void* userdata) {
    pa_blocking* p = (pa_blocking*) userdata;
    
    if (info) {
        p->hw_volume = (info->flags & PA_SINK_HW_VOLUME_CTRL) != 0;
        p->operation_success = 1;
        pa_threaded_mainloop_signal(p->mainloop, 0);
    }
}

static void sink_input_info_cb(pa_context* c, const pa_sink_input_info* info, int eol, void* userdata) {
    pa_blocking* p = (pa_blocking*) userdata;
    
    if (info) {
        p->sink_id = info->sink;
        p->operation_success = 1;
        pa_threaded_mainloop_signal(p->mainloop, 0);
    }
}

int pa_blocking_has_hw_volume(pa_blocking *p, int *rerror) {
    pa_operation *o = NULL;
    pa_stream *s = NULL;
    uint32_t idx;
    int result = -1;
    int sink = -1;

    assert(p);

    if (p->hw_volume != -1) {
        return p->hw_volume;
    }

    CHECK_VALIDITY_RETURN_ANY(rerror, p->direction == PA_STREAM_PLAYBACK, PA_ERR_BADSTATE, -1);

    pa_threaded_mainloop_lock(p->mainloop);
    CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);

    CHECK_SUCCESS_GOTO(p, rerror, ((idx = pa_stream_get_index (p->stream)) != PA_INVALID_INDEX), unlock_and_fail);

    s = p->stream;
    assert(s);

    /* determine the sink id */
    if (p->sink_id == -1) {
        o = pa_context_get_sink_input_info(p->context, idx, sink_input_info_cb, p);

        CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);
        p->operation_success = 0;
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(p->mainloop);
            CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
        }
        CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

        pa_operation_unref(o);
    }

    //fprintf(stderr, "success getting sink_id=%d\n", p->sink_id);

    if (p->sink_id != -1) {
        /* once we have the sink id, query it for hw volume info */
        o = pa_context_get_sink_info_by_index(p->context, p->sink_id, sink_info_cb, p);
    
        CHECK_SUCCESS_GOTO(p, rerror, o, unlock_and_fail);
        p->operation_success = 0;
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(p->mainloop);
            CHECK_DEAD_GOTO(p, rerror, unlock_and_fail);
        }
        CHECK_SUCCESS_GOTO(p, rerror, p->operation_success, unlock_and_fail);

        pa_operation_unref(o);
    }
    
    //fprintf(stderr, "success getting hw_volume=%d\n", p->hw_volume);
    
    pa_threaded_mainloop_unlock(p->mainloop);
    return p->hw_volume == -1 ? 0 : p->hw_volume;

unlock_and_fail:

    if (o) {
        pa_operation_cancel(o);
        pa_operation_unref(o);
    }

    pa_threaded_mainloop_unlock(p->mainloop);
    return 0;
}
