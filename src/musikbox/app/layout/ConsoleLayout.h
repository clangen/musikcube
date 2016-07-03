//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <cursespp/TextInput.h>

#include <app/window/LogWindow.h>
#include <app/window/OutputWindow.h>
#include <app/window/TransportWindow.h>
#include <app/window/ResourcesWindow.h>
#include <app/window/ShortcutsWindow.h>

#include <vector>

#include <core/audio/ITransport.h>

#include <boost/shared_ptr.hpp>

namespace musik {
    namespace box {
        class ConsoleLayout : public cursespp::LayoutBase, public sigslot::has_slots<> {
            public:
                ConsoleLayout(
                    musik::core::audio::ITransport& transport, 
                    musik::core::LibraryPtr library);

                ~ConsoleLayout();

                virtual void Layout();
                virtual void Show();
                virtual void ProcessMessage(cursespp::IMessage &message);

            private:
                void UpdateWindows();

                void OnEnterPressed(cursespp::TextInput* input);

                void ListPlugins() const;
                bool ProcessCommand(const std::string& cmd);
                bool PlayFile(const std::vector<std::string>& args);
                void Pause();
                void Stop();
                void Seek(const std::vector<std::string>& args);
                void SetVolume(const std::vector<std::string>& args);
                void SetVolume(float volume);
                void Help();

                std::shared_ptr<LogWindow> logs;
                std::shared_ptr<cursespp::TextInput> commands;
                std::shared_ptr<OutputWindow> output;
                std::shared_ptr<ResourcesWindow> resources;
                std::shared_ptr<ShortcutsWindow> shortcuts;
                musik::core::audio::ITransport& transport;
                musik::core::LibraryPtr library;
        };
    }
}