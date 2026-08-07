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

/** @file Plugins.h
 *  @brief Free functions that manage the plugin system lifecycle.
 *  @details Initializes, starts and shuts down all loaded plugins, and exposes
 *      the shared IEnvironment used to communicate with them. */

#include <musikcore/library/ILibrary.h>
#include <musikcore/sdk/IEnvironment.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/runtime/IMessage.h>

/** @namespace musik::core::plugin
 *  @brief Plugin lifecycle and environment helpers. */
namespace musik { namespace core { namespace plugin {

    /** @brief Initializes the plugin system (called once at startup). */
    void Init();

    /** @brief Starts all plugins with the application's core services.
     *  @param messageQueue The queue plugins use for async messages.
     *  @param playback The playback service passed to plugins.
     *  @param library The primary library passed to plugins. */
    void Start(
        musik::core::runtime::IMessageQueue* messageQueue,
        musik::core::sdk::IPlaybackService* playback,
        musik::core::ILibraryPtr library);

    /** @return The shared plugin environment. */
    musik::core::sdk::IEnvironment& Environment();

    /** @brief Shuts down all plugins and releases their resources. */
    void Shutdown();

} } }
