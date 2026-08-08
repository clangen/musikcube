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
 * @file RemoteLibrarySettingsLayout.h
 * @brief Layout for configuring the remote library server connection.
 * @details Edits the HTTP/WebSocket server host, port and password, TLS
 *          settings and transcoding options, and persists them to
 *          preferences.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/Checkbox.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/support/Preferences.h>

namespace musik { namespace cube {
    /**
     * @brief Remote library server settings layout.
     * @details Provides inputs for the server host, ports and password,
     *          toggles for TLS and transcoding, and dropdowns for the
     *          transcoder format and bitrate. Changes are synced with
     *          persistent preferences.
     */
    class RemoteLibrarySettingsLayout: public cursespp::LayoutBase, public sigslot::has_slots<>  {
        public:
            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(RemoteLibrarySettingsLayout)

            /**
             * @brief Creates the settings layout.
             */
            RemoteLibrarySettingsLayout();
            /**
             * @brief Destroys the layout and its child views.
             */
            virtual ~RemoteLibrarySettingsLayout();

            /**
             * @brief Loads the persisted server settings.
             */
            void LoadPreferences();
            /**
             * @brief Saves the current values to preferences.
             */
            void SavePreferences();

            /* IWindow */
            /**
             * @brief Positions and lays out the child windows.
             */
            void OnLayout() override;

        private:
            void InitializeWindows();
            void SyncPreferencesAndLayout();

            void OnTlsCheckboxChanged(cursespp::Checkbox* cb, bool checked);
            void OnActivateTranscoderFormat(cursespp::TextLabel* tl);
            void OnActivateTranscoderBitrate(cursespp::TextLabel* tl);

            musik::core::ILibraryPtr library; /**< the library whose server settings are edited */

            std::shared_ptr<musik::core::Preferences> prefs; /**< persistent preferences */
            std::shared_ptr<cursespp::TextLabel> httpPortLabel, wssPortLabel, hostLabel, pwLabel; /**< setting labels */
            std::shared_ptr<cursespp::TextInput> httpPortInput, wssPortInput, hostInput, pwInput; /**< setting inputs */
            std::shared_ptr<cursespp::Checkbox> wssTlsCheckbox, httpTlsCheckbox, transcoderCheckbox; /**< setting toggles */
            std::shared_ptr<cursespp::TextLabel> transcoderFormatDropdown, transcoderBitrateDropdown; /**< transcoder dropdowns */
    };
} }
