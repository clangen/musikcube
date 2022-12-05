//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <stdafx.h>

#include <stdlib.h>
#include <time.h>

#include <cstdio>

#include <cursespp/App.h>
#include <cursespp/Screen.h>

#include <app/layout/MainLayout.h>
#include <app/util/ConsoleLogger.h>
#include <app/util/GlobalHotkeys.h>
#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/debug.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/support/Common.h>
#include <musikcore/net/PiggyWebSocketClient.h>
#include <musikcore/support/PiggyDebugBackend.h>

#ifdef WIN32
#include <cursespp/Win32Util.h>
#include "resource.h"
#endif

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::net;
using namespace musik::cube;
using namespace cursespp;

#define MIN_WIDTH 54
#define MIN_HEIGHT 12

namespace keys = musik::cube::prefs::keys;

#if defined(WIN32) && defined(PDCURSES_WINGUI)
int _main(int argc, wchar_t* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    return App::Running("musikcube") ? 0 : _main(0, 0);
}

int _main(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif

    srand((unsigned int) time(0));

    /* ensure we have the correct locale loaded */
    core::i18n::Locale::Instance().Initialize(core::GetApplicationDirectory() + "/locales/");

#ifdef WIN32
    AddDllDirectory(u8to16(core::GetPluginDirectory()).c_str());
    #ifdef __PDCURSES__
        PDC_set_resize_limits(MIN_HEIGHT, 1000, MIN_WIDTH, 1000);
        resize_term(26, 100); /* must be before app init */
    #endif
#endif

    std::string errorFn = core::GetDataDirectory() + "stderr.txt";
#ifdef WIN32
    _wfreopen(u8to16(errorFn).c_str(), L"w", stderr);
#else
    freopen(errorFn.c_str(), "w", stderr);
#endif

    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    auto consoleLogger = new ConsoleLogger(Window::MessageQueue());

    std::vector<debug::IBackend*> debuggerBackends = { 
        new debug::SimpleFileBackend(), consoleLogger
    };

    if (prefs->GetBool(core::prefs::keys::PiggyEnabled, false)) {
        auto piggyClient = PiggyWebSocketClient::Instance(&Window::MessageQueue());
        piggyClient->Connect(prefs->GetString(core::prefs::keys::PiggyHostname, "localhost"));
        debuggerBackends.push_back(new PiggyDebugBackend(piggyClient));
    }

    debug::Start(debuggerBackends);

    plugin::Init();

    LibraryFactory::Initialize(Window::MessageQueue());
    auto library = std::make_shared<library::MasterLibrary>();

#ifdef BUILD_STANDALONE
    /* when we build a standalone binary we make sure to also package the terminfo used
    by the version of ncurses we compile against. if the target system has ncurses5, the
    terminfo format will not be compatible and the app will not run */
    const std::string terminfo = core::GetApplicationDirectory() + "/share/terminfo/";
    setenv("TERMINFO", terminfo.c_str(), 1);
#endif

    {
        PlaybackService playback(Window::MessageQueue(), library);

        GlobalHotkeys globalHotkeys(playback, library);
        Window::SetNavigationKeys(Hotkeys::NavigationKeys());

        plugin::Start(&Window::MessageQueue(), &playback, library);

#ifdef WIN32
        auto font = GetApplicationDirectory() + "fonts/SourceCodePro-Medium.ttf";
        if (App::RegisterFont(font)) {
            App::SetDefaultFontface("Source Code Pro Medium");
            App::SetDefaultMenuVisibility(false);
        }
#endif

        App app("musikcube"); /* must be before layout creation */

#ifdef WIN32
        app.SetIcon(IDI_ICON1);
        app.SetSingleInstanceId("musikcube");
#endif

        app.SetQuitKey(prefs->GetString(keys::AppQuitKey, "^D"));

        /* fire up the indexer if configured to run on startup */
        if (prefs->GetBool(core::prefs::keys::SyncOnStartup, true)) {
            library->Indexer()->Schedule(IIndexer::SyncType::All);
        }

        /* set color mode (basic, palette, rgb) */
        Colors::Mode colorMode = Colors::RGB;

        if (prefs->GetBool(keys::DisableCustomColors.c_str(), false)) {
            colorMode = Colors::Basic;
        }
#ifndef WIN32
        else if (prefs->GetBool(keys::UsePaletteColors.c_str(), true)) {
            colorMode = Colors::Palette;
        }
#endif

        Colors::BgType bgType = Colors::Theme;
        if (prefs->GetBool(keys::InheritBackgroundColor.c_str(), false)) {
            bgType = Colors::Inherit;
        }

        app.SetColorMode(colorMode);
        app.SetColorBackgroundType(bgType);

        /* theme */
        std::string colorTheme = prefs->GetString(keys::ColorTheme);
        if (colorTheme.size()) {
            app.SetColorTheme(colorTheme);
        }

        /* tray icon */
        app.SetMinimizeToTray(prefs->GetBool(keys::MinimizeToTray, false));
        if (prefs->GetBool(keys::StartMinimized, false)) {
            app.Minimize();
        }

        app.SetMinimumSize(MIN_WIDTH, MIN_HEIGHT);

        /* main layout */
        auto mainLayout = std::make_shared<MainLayout>(app, consoleLogger, playback, library);

        mainLayout->Start();

        app.SetKeyHandler([&](const std::string& kn) -> bool {
            if (app.IsOverlayVisible()) {
                return false;
            }

            return globalHotkeys.Handle(kn);
        });

        /* blocking event loop */
        app.Run(mainLayout);

        /* done with the app */
        mainLayout->Shutdown();

#ifdef WIN32
        win32::HideMainWindow();
#endif

        library->Indexer()->Shutdown();
    }

    PiggyWebSocketClient::Shutdown();
    LibraryFactory::Instance().Shutdown();
    vis::Shutdown();
    plugin::Shutdown();
    debug::Shutdown();

    return 0;
}
