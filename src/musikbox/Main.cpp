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

#include "stdafx.h"

#include <cursespp/App.h>

#include <app/layout/ConsoleLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/SettingsLayout.h>
#include <app/layout/MainLayout.h>
#include <app/util/GlobalHotkeys.h>
#include <app/util/Hotkeys.h>
#include <app/service/PlaybackService.h>

#include <core/library/LibraryFactory.h>
#include <core/audio/GaplessTransport.h>

#include <cstdio>

#ifdef WIN32
#undef MOUSE_MOVED
#endif

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;

#ifdef WIN32
int _main(int argc, _TCHAR* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    _main(0, 0);
}

int _main(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
#ifndef WIN32
    #if 1 /*DEBUG*/
        freopen("/tmp/musikbox.log", "w", stderr);
    #else
        freopen("/dev/null", "w", stderr);
    #endif
#endif

#ifdef __PDCURSES__
    PDC_set_resize_limits(12, 60, 60, 250);
    resize_term(26, 100); /* must be before app init */
#endif

    musik::debug::init();

    LibraryPtr library = LibraryFactory::Libraries().at(0);

    GaplessTransport transport;
    PlaybackService playback(library, transport);

    GlobalHotkeys globalHotkeys(playback, library);

    {
        App app("musikbox"); /* must be before layout creation */

        using Layout = std::shared_ptr<LayoutBase>;
        using Main = std::shared_ptr<MainLayout>;

        Layout libraryLayout(new LibraryLayout(playback, library));
        Layout consoleLayout(new ConsoleLayout(transport, library));
        Layout settingsLayout(new SettingsLayout(library));

        Main mainLayout(new MainLayout());
        mainLayout->Layout();
        mainLayout->SetMainLayout(libraryLayout);

        app.SetKeyHandler([&](const std::string& kn) {
            if (Hotkeys::Is(Hotkeys::NavigateConsole, kn)) {
                mainLayout->SetMainLayout(consoleLayout);
                return true;
            }
            else if (Hotkeys::Is(Hotkeys::NavigateLibrary, kn)) {
                mainLayout->SetMainLayout(libraryLayout);
                return true;
            }
            else if (Hotkeys::Is(Hotkeys::NavigateSettings, kn)) {
                mainLayout->SetMainLayout(settingsLayout);
                return true;
            }

            return globalHotkeys.Handle(kn);
        });

        app.SetResizeHandler([&]() {
            mainLayout->Layout();
        });

        app.Run(mainLayout);
    }

    endwin();

    LibraryFactory::Instance().Shutdown();
    musik::debug::deinit();

    return 0;
}
