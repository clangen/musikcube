//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/**
 * @file TrackRowRenderers.h
 * @brief Renderers that format a track row for list views.
 * @details Defines the renderer function type and the built-in row formats
 *          used by the album sort and now playing track lists.
 */

#include <musikcore/library/track/Track.h>

namespace musik {
    namespace cube {
        /**
         * @brief Namespace holding the track row renderer registry.
         */
        namespace TrackRowRenderers {
            /**
             * @brief The built-in renderer styles.
             */
            enum class Type {
                AlbumSort,   /**< album/artist/title with track numbers */
                NowPlaying,  /**< compact format for the now playing list */
            };

            /**
             * @brief How the track number is derived.
             */
            enum class TrackNumType {
                Metadata,    /**< use the metadata track number */
                Sequential,  /**< use the sequential index in the list */
            };

            /**
             * @brief Function that renders a track into a display string.
             * @param metadata the track to render
             * @param index the index of the track in the list
             * @param width the available display width
             * @param type how the track number is derived
             * @return the formatted row string
             */
            using Renderer = std::function<std::string(
                musik::core::TrackPtr /*metadata*/,
                size_t /*index*/,
                size_t /*width*/,
                TrackNumType /*type*/
            )>;

            /**
             * @brief Returns the renderer for the given style.
             * @param type the renderer style
             * @return the renderer function
             */
            extern const Renderer Get(Type type);
        }
    }
}
