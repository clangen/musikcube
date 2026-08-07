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

/** @file IPlugin.h @brief Defines the IPlugin interface implemented by all musikcube plugins. */
#pragma once

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief The base interface implemented by every plugin loaded by the
     *  application, providing identity and lifecycle methods. */
    class IPlugin {
        public:
            /** @brief Releases the plugin; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns the display name of the plugin.
             *  @return The plugin name. */
            virtual const char* Name() = 0;

            /** @brief Returns the version of the plugin.
             *  @return The plugin version string. */
            virtual const char* Version() = 0;

            /** @brief Returns the author of the plugin.
             *  @return The author name. */
            virtual const char* Author() = 0;

            /** @brief Returns the globally unique identifier of the plugin.
             *  @return The plugin guid. */
            virtual const char* Guid() = 0;

            /** @brief Returns whether the plugin exposes a configuration UI.
             *  @return True if the plugin is configurable. */
            virtual bool Configurable() = 0;

            /** @brief Launches the plugin's configuration UI. */
            virtual void Configure() = 0;

            /** @brief Reloads the plugin's configuration from disk. */
            virtual void Reload() = 0;

            /** @brief Returns the SDK version the plugin was built against.
             *  @return The SDK version. */
            virtual int SdkVersion() = 0;
    };

} } }

