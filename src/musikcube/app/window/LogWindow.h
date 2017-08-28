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

#include <mutex>

#include <sigslot/sigslot.h>

#include <core/debug.h>
#include <core/runtime/IMessage.h>

#include <cursespp/ScrollableWindow.h>
#include <cursespp/SimpleScrollAdapter.h>

namespace musik {
    namespace cube {
        class LogWindow :
            public cursespp::ScrollableWindow,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<LogWindow>,
#endif
            public sigslot::has_slots<>
        {
            public:
                LogWindow(cursespp::IWindow *parent = NULL);
                virtual ~LogWindow();

                void ClearContents();

                virtual void ProcessMessage(musik::core::runtime::IMessage &message);
                virtual void OnVisibilityChanged(bool visible);

            protected:
                virtual cursespp::IScrollAdapter& GetScrollAdapter();

            private:
                void Update();

                void OnLogged(
                    musik::debug::log_level level,
                    std::string tag,
                    std::string message);

                struct LogEntry {
                    int level;
                    std::string tag;
                    std::string message;
                };

                std::mutex pendingMutex;
                std::vector<LogEntry*> pending;
                cursespp::SimpleScrollAdapter* adapter;
        };
    }
}
