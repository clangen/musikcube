//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>
#include <cursespp/Checkbox.h>
#include <core/library/ILibrary.h>
#include <core/support/Preferences.h>

namespace musik { namespace cube {
    class RemoteLibrarySettingsLayout: public cursespp::LayoutBase  {
        public:
            RemoteLibrarySettingsLayout(musik::core::ILibraryPtr library);

            virtual ~RemoteLibrarySettingsLayout();

            virtual void OnVisibilityChanged(bool visible);

        protected:
            virtual void OnLayout();

        private:
            void InitializeWindows();
            void LoadPreferences();
            void SavePreferences();

            musik::core::ILibraryPtr library;

            std::shared_ptr<musik::core::Preferences> prefs;
            std::shared_ptr<cursespp::TextLabel> httpPortLabel, wssPortLabel, hostLabel, pwLabel;
            std::shared_ptr<cursespp::TextInput> httpPortInput, wssPortInput, hostInput, pwInput;
            std::shared_ptr<cursespp::Checkbox> ipv6Cb;
    };
} }
