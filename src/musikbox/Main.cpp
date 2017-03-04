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

#include <stdlib.h>
#include <time.h>

#include <cstdio>

#include <cursespp/App.h>
#include <cursespp/Screen.h>

#include <app/layout/ConsoleLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/SettingsLayout.h>
#include <app/layout/MainLayout.h>
#include <app/util/GlobalHotkeys.h>
#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>
#include <core/audio/PlaybackService.h>

#include <glue/audio/MasterTransport.h>

#include <core/library/LibraryFactory.h>
#include <core/plugin/Plugins.h>
#include <core/support/PreferenceKeys.h>
#include <core/audio/Visualizer.h>
#include <core/support/Common.h>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#ifdef WIN32
    #include <cursespp/Win32Util.h>
    #include "resource.h"
    #undef MOUSE_MOVED
#endif

using namespace musik::glue;
using namespace musik::glue::audio;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::box;
using namespace cursespp;

#define MIN_WIDTH 48
#define MIN_HEIGHT 12

#ifdef WIN32
int _main(int argc, wchar_t* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    _main(0, 0);
}

int _main(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    srand((unsigned int) time(0));

    /* the following allows boost::filesystem to use utf8 on Windows */
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);

#ifndef WIN32
    #if 1 /*DEBUG*/
        freopen("/tmp/musikbox.log", "w", stderr);
    #else
        freopen("/dev/null", "w", stderr);
    #endif
#endif

#ifdef __PDCURSES__
    PDC_set_resize_limits(MIN_HEIGHT, 1000, MIN_WIDTH, 1000);
    resize_term(26, 100); /* must be before app init */
#endif

    musik::core::MigrateOldDataDirectory();

    musik::debug::init();

    ILibraryPtr library = LibraryFactory::Libraries().at(0);
    library->SetMessageQueue(Window::MessageQueue());

    MasterTransport transport;
    PlaybackService playback(Window::MessageQueue(), library, transport);

    GlobalHotkeys globalHotkeys(playback, library);

    musik::core::plugin::InstallDependencies(library);

    {
        App app("musikbox"); /* must be before layout creation */

#ifdef WIN32
        app.SetIcon(IDI_ICON1);
#endif

        auto prefs = Preferences::ForComponent(
            musik::core::prefs::components::Settings);

        app.SetCustomColorsDisabled(prefs->GetBool(
            musik::box::prefs::keys::DisableCustomColors.c_str(), false));

        app.SetMinimumSize(MIN_WIDTH, MIN_HEIGHT);

        using Layout = std::shared_ptr<LayoutBase>;
        using Main = std::shared_ptr<MainLayout>;

        Layout libraryLayout(new LibraryLayout(playback, library));
        Layout consoleLayout(new ConsoleLayout(transport, library));
        Layout settingsLayout(new SettingsLayout(library, transport));

        Main mainLayout(new MainLayout());

        std::vector<std::string> paths;
        library->Indexer()->GetPaths(paths);

        mainLayout->SetMainLayout(paths.size() > 0
            ? libraryLayout : settingsLayout);

        app.SetKeyHandler([&](const std::string& kn) {
            if (app.IsOverlayVisible()) {
                return false;
            }
            else if (Hotkeys::Is(Hotkeys::NavigateConsole, kn)) {
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

        app.Run(mainLayout);

#ifdef WIN32
        win32::HideMainWindow();
#endif
    }

    musik::core::audio::vis::HideSelectedVisualizer();
    musik::core::plugin::UninstallDependencies();

    LibraryFactory::Instance().Shutdown();

    musik::debug::deinit();

    return 0;
}
