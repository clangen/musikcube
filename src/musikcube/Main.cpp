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

#include <core/audio/PlaybackService.h>
#include <core/audio/Visualizer.h>
#include <core/debug.h>
#include <core/i18n/Locale.h>
#include <core/library/LibraryFactory.h>
#include <core/plugin/Plugins.h>
#include <core/support/PreferenceKeys.h>
#include <core/sdk/constants.h>
#include <core/support/Common.h>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#ifdef WIN32
#include <cursespp/Win32Util.h>
#include "resource.h"
#endif

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
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

    /* the following allows boost::filesystem to use utf8 on Windows */
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);

    /* ensure we have the correct locale loaded */
    musik::core::i18n::Locale::Instance().Initialize(musik::core::GetApplicationDirectory() + "/locales/");

#ifdef WIN32
    AddDllDirectory(u8to16(musik::core::GetPluginDirectory()).c_str());
    #ifdef __PDCURSES__
        PDC_set_resize_limits(MIN_HEIGHT, 1000, MIN_WIDTH, 1000);
        resize_term(26, 100); /* must be before app init */
    #endif
#endif

    std::string errorFn = musik::core::GetDataDirectory() + "stderr.txt";
    freopen(errorFn.c_str(), "w", stderr);

    auto fileLogger = new musik::debug::SimpleFileBackend();
    auto consoleLogger = new ConsoleLogger(Window::MessageQueue());
    musik::debug::Start({ fileLogger, consoleLogger });

    ILibraryPtr library = LibraryFactory::Default();
    library->SetMessageQueue(Window::MessageQueue());

    {
        auto prefs = Preferences::ForComponent(
            musik::core::prefs::components::Settings);

        PlaybackService playback(Window::MessageQueue(), library);

        GlobalHotkeys globalHotkeys(playback, library);
        Window::SetNavigationKeys(Hotkeys::NavigationKeys());

        musik::core::plugin::Init(
            &Window::MessageQueue(), &playback, library);

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
        if (prefs->GetBool(musik::core::prefs::keys::SyncOnStartup, true)) {
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
        mainLayout->Stop();

#ifdef WIN32
        win32::HideMainWindow();
#endif

        library->Indexer()->Stop();
    }

    musik::core::audio::vis::HideSelectedVisualizer();
    musik::core::plugin::Deinit();

    LibraryFactory::Instance().Shutdown();

    musik::debug::Stop();

    return 0;
}
