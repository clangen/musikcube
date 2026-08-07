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

/** @file Messages.h
 *  @brief Core runtime message type constants.
 *  @details Defines the message type ids used by the core application. Custom
 *      message types may start at the User value. */

/** @namespace musik::core::message
 *  @brief Runtime message type constants. */
namespace musik {
    namespace core {
        namespace message {
            /** @brief First reserved core message id. */
            static const int First = 512;

            /** @brief A playlist's tracks were modified. */
            static const int PlaylistModified       = First + 1;
            /** @brief A playlist was created. */
            static const int PlaylistCreated        = First + 2;
            /** @brief A playlist was renamed. */
            static const int PlaylistRenamed        = First + 3;
            /** @brief A playlist was deleted. */
            static const int PlaylistDeleted        = First + 4;
            /** @brief The plugin environment was updated. */
            static const int EnvironmentUpdated     = First + 5;
            /** @brief The equalizer settings were updated. */
            static const int EqualizerUpdated       = First + 6;

            /** @brief First id available for custom (user-defined) messages. */
            static const int User                   = 4096;
        }
    }
}
