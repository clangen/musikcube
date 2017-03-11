/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#pragma once

#include <sys/types.h>
#include <stdbool.h>

#include <pulse/sample.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>
#include <pulse/cdecl.h>
#include <pulse/version.h>

PA_C_DECL_BEGIN

/** \struct pa_blocking
 * An opaque simple connection object */
typedef struct pa_blocking pa_blocking;

/** Create a new connection to the server. */
pa_blocking* pa_blocking_new(
    const char *server,                 /**< Server name, or NULL for default */
    const char *name,                   /**< A descriptive name for this client (application name, ...) */
    pa_stream_direction_t dir,          /**< Open this stream for recording or playback? */
    const char *dev,                    /**< Sink (resp. source) name, or NULL for default */
    const char *stream_name,            /**< A descriptive name for this stream (application name, song title, ...) */
    const pa_sample_spec *ss,           /**< The sample type to use */
    const pa_channel_map *map,          /**< The channel map to use, or NULL for default */
    const pa_buffer_attr *attr,         /**< Buffering attributes, or NULL for default */
    int *error                          /**< A pointer where the error code is stored when the routine returns NULL. It is OK to pass NULL here. */
    );

/** Close and free the connection to the server. The connection object becomes invalid when this is called. */
void pa_blocking_free(pa_blocking *s);

/** Write some data to the server. */
int pa_blocking_write(pa_blocking *s, const void *data, size_t bytes, int *error);

/** Wait until all data already written is played by the daemon. */
int pa_blocking_drain(pa_blocking *s, int *error);

/** Read some data from the server. This function blocks until \a bytes amount
 * of data has been received from the server, or until an error occurs.
 * Returns a negative value on failure. */
int pa_blocking_read(
    pa_blocking *s, /**< The connection object. */
    void *data,     /**< A pointer to a buffer. */
    size_t bytes,   /**< The number of bytes to read. */
    int *error
    /**< A pointer where the error code is stored when the function returns
     * a negative value. It is OK to pass NULL here. */
    );

/** Return the playback or record latency. */
pa_usec_t pa_blocking_get_latency(pa_blocking *s, int *error);

/** Flush the playback or record buffer. This discards any audio in the buffer. */
int pa_blocking_flush(pa_blocking *s, int *error);

/** Set the output volume of the stream. */
int pa_blocking_set_volume(pa_blocking *p, int volume, int *rerror);

/** returns 1 if the stream has hardware volume, false otherwise */
int pa_blocking_has_hw_volume(pa_blocking *p, int *rerror);

PA_C_DECL_END

