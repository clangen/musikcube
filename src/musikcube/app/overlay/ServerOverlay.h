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
 * @file ServerOverlay.h
 * @brief Overlay for configuring the local streaming server plugin.
 * @details Lets the user enable the WebSocket, HTTP and sync transfer
 *          endpoints, configure ports, an optional password and the
 *          transcoding cache and max concurrent transfer settings.
 */

#include <functional>

#include <musikcore/sdk/IPlugin.h>
#include <musikcore/support/Preferences.h>

#include <cursespp/Checkbox.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/ShortcutsWindow.h>

namespace musik {
    namespace cube {
        /**
         * @brief Streaming server configuration overlay.
         * @details Shows checkboxes and inputs for the server endpoints, ports
         *          and transfer limits. Saving persists the values to
         *          preferences and applies them to the plugin.
         */
        class ServerOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
    {
        public:
            using Callback = std::function<void()>;
            using Plugin = std::shared_ptr<musik::core::sdk::IPlugin>;
            using Prefs = std::shared_ptr<musik::core::Preferences>;

            /**
             * @brief Shows the server configuration overlay.
             * @param callback invoked when the overlay is closed
             */
            static void Show(Callback callback);
            /**
             * @brief Finds the installed streaming server plugin.
             * @return the server plugin, or nullptr if not installed
             */
            static std::shared_ptr<musik::core::sdk::IPlugin> FindServerPlugin();

            /**
             * @brief Positions and lays out the child views.
             */
            virtual void Layout();
            /**
             * @brief Handles keyboard input.
             * @param key the key sequence that was pressed
             * @return true if the event was consumed
             */
            virtual bool KeyPress(const std::string& key);

        private:
            /**
             * @brief Creates the overlay for the given plugin.
             * @param callback invoked when the overlay is closed
             * @param plugin the server plugin being configured
             */
            ServerOverlay(Callback callback, Plugin plugin);

            void RecalculateSize();
            void InitViews();
            bool Save();
            void Load();

            Callback callback;                                              /**< invoked when the overlay closes */
            Plugin plugin;                                                  /**< the server plugin being configured */
            Prefs prefs;                                                    /**< preferences used to persist the settings */
            int width, height, x, y;                                        /**< cached overlay geometry */

            std::shared_ptr<cursespp::TextLabel> titleLabel;                /**< the overlay title label */
            std::shared_ptr<cursespp::Checkbox> enableWssCb, enableHttpCb, enableSyncTransCb; /**< endpoint enable checkboxes */
            std::shared_ptr<cursespp::Checkbox> ipv6Cb;                     /**< IPv6 support checkbox */
            std::shared_ptr<cursespp::TextLabel> wssPortLabel, httpPortLabel, pwLabel, transCacheLabel, maxTransLabel; /**< static labels */
            std::shared_ptr<cursespp::TextInput> wssPortInput, httpPortInput, pwInput, transCacheInput, maxTransInput; /**< value inputs */

            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;           /**< the shortcuts window */
        };
    }
}
