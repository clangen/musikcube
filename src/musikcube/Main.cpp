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

#include <stdafx.h>
#include <musikcore/paths.hpp>

#include <musikcore/preferences.hpp>

#include <stdlib.h>
#include <time.h>

#include <cstdio>

#include <cursespp/App.h>
#include <cursespp/Screen.h>

#include <app/layout/MainLayout.h>
#include <app/util/ConsoleLogger.h>
#include <app/util/GlobalHotkeys.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>

#include <common/runtime/Message.h>
#include <app/util/Locale.h>
#include <musikcore/indexer.hpp>
#include <musikcore/sdk/constants.h>
#include <musikcore/support/Common.h>

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

#if defined(WIN32) && !defined(PDCURSES_WINCON)
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
    api::LoadLocales(api::ReadPath(MC_PATH_APPLICATION) + "/locales/");

#ifdef WIN32
    AddDllDirectory(u8to16(api::ReadPath(MC_PATH_PLUGINS)).c_str());
    #ifdef __PDCURSES__
        PDC_set_resize_limits(MIN_HEIGHT, 1000, MIN_WIDTH, 1000);
        resize_term(26, 100); /* must be before app init */
    #endif
#endif

    std::string errorFn = api::ReadPath(MC_PATH_DATA) + "stderr.txt";
#ifdef WIN32
    _wfreopen(u8to16(errorFn).c_str(), L"w", stderr);
#else
    freopen(errorFn.c_str(), "w", stderr);
#endif

    auto prefs = api::Preferences { MC_PREFERENCE_SETTINGS };

#if defined(BUILD_STANDALONE) && !defined(__APPLE__)
    /* when we build a standalone binary we make sure to also package the terminfo used
    by the version of ncurses we compile against. if the target system has ncurses5, the
    terminfo format will not be compatible and the app will not run */
    const std::string terminfo = api::ReadPath(MC_PATH_APPLICATION) + "/share/terminfo/";
    setenv("TERMINFO", terminfo.c_str(), 1);
#endif

    {
        /* The context outlives the app and every view that borrows it. */
        ConsoleLogger consoleLogger;
        mc_context_options apiOptions = MC_CONTEXT_OPTIONS_DEFAULT;
        apiOptions.flags = MC_CONTEXT_MANAGED_RUNTIME | MC_CONTEXT_ENABLE_LOGGING;
        apiOptions.log = ConsoleLogger::OnLog;
        apiOptions.log_user_data = &consoleLogger;
        apiOptions.wakeup_user_data = &Window::MessageQueue();
        apiOptions.wakeup = [](void* data) {
            auto& queue = *static_cast<runtime::IMessageQueue*>(data);
            queue.Broadcast(runtime::Message::Create(nullptr, cube::message::DispatchCoreRequests));
        };
        api::Context musikcore(apiOptions);
        Window::SetNavigationKeys(Hotkeys::NavigationKeys());

#ifdef WIN32
        auto font = api::ReadPath(MC_PATH_APPLICATION) + "fonts/SourceCodePro-Medium.ttf";
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

        app.SetQuitKey(prefs.GetString(keys::AppQuitKey, "^D"));

        /* fire up the indexer if configured to run on startup */
        if (prefs.GetBool(MC_PREFERENCE_KEY_SYNC_ON_STARTUP, true) &&
            musikcore.GetLibraryStatus().kind == MC_LIBRARY_LOCAL)
        {
            api::Indexer(musikcore).Schedule(MC_INDEXER_SCAN_ALL);
        }

        /* set color mode (basic, palette, rgb) */
        Colors::Mode colorMode = Colors::RGB;

        if (prefs.GetBool(keys::DisableCustomColors.c_str(), false)) {
            colorMode = Colors::Basic;
        }
#ifndef WIN32
        else if (prefs.GetBool(keys::UsePaletteColors.c_str(), true)) {
            colorMode = Colors::Palette;
        }
#endif

        Colors::BgType bgType = Colors::Theme;
        if (prefs.GetBool(keys::InheritBackgroundColor.c_str(), false)) {
            bgType = Colors::Inherit;
        }

        app.SetColorMode(colorMode);
        app.SetColorBackgroundType(bgType);

        /* theme */
        std::string colorTheme = prefs.GetString(keys::ColorTheme);
        if (colorTheme.size()) {
            app.SetColorTheme(colorTheme);
        }

        /* tray icon */
        app.SetMinimizeToTray(prefs.GetBool(keys::MinimizeToTray, false));
        if (prefs.GetBool(keys::StartMinimized, false)) {
            app.Minimize();
        }

        app.SetMinimumSize(MIN_WIDTH, MIN_HEIGHT);

        /* main layout */
        auto mainLayout = std::make_shared<MainLayout>(app, &consoleLogger, musikcore);
        GlobalHotkeys globalHotkeys(musikcore);

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
    }

    return 0;
}
