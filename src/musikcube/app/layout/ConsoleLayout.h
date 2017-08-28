//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/ScrollableWindow.h>

#include <app/window/LogWindow.h>
#include <app/window/TransportWindow.h>

#include <vector>

#include <core/audio/ITransport.h>

#include "ITopLevelLayout.h"

namespace musik {
    namespace cube {
        class ConsoleLayout :
            public cursespp::LayoutBase,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<ConsoleLayout>,
#endif
            public ITopLevelLayout,
            public sigslot::has_slots<>
        {
            public:
                ConsoleLayout(
                    musik::core::audio::ITransport& transport,
                    musik::core::ILibraryPtr library);

                ~ConsoleLayout();

                virtual void SetShortcutsWindow(
                    cursespp::ShortcutsWindow* shortcuts);

            protected:
                virtual void OnLayout();

            private:
                void OnEnterPressed(cursespp::TextInput* input);

                void ListPlugins();
                bool ProcessCommand(const std::string& cmd);
                bool PlayFile(const std::vector<std::string>& args);
                void Pause();
                void Stop();
                void Seek(const std::vector<std::string>& args);
                void SetVolume(const std::vector<std::string>& args);
                void SetVolume(float volume);
                void Help();

                void WriteOutput(const std::string& str, int64_t attrs = -1);

                std::shared_ptr<LogWindow> logs;
                std::shared_ptr<cursespp::TextInput> commands;
                std::shared_ptr<cursespp::ScrollableWindow> output;
                std::shared_ptr<cursespp::SimpleScrollAdapter> outputAdapter;
                musik::core::audio::ITransport& transport;
                musik::core::ILibraryPtr library;
        };
    }
}