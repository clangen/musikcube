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

/* required to take advantage of system theming */
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <pch.h>

#include <win32cpp/Application.hpp>

#include <core/library/LibraryFactory.h>
#include <core/audio/PlaybackService.h>
#include <core/support/Preferences.h>
#include <core/audio/Visualizer.h>

#include <app/view/MainWindow.h>
#include <app/controller/MainController.h>

#include <chrono>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::win;
using namespace win32cpp;

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand) {
    Preferences::LoadPluginPreferences();

    Application::Initialize(instance, previousInstance, commandLine, showCommand);
    Application& app = Application::Instance();

    MainWindow mainWindow(_TT("musikwin").c_str());
    mainWindow.Initialize();
    mainWindow.Resize(640, 400);
    mainWindow.MoveTo(200, 200);

    ILibraryPtr library = LibraryFactory::Default();
    library->SetMessageQueue(mainWindow.Queue());

    {
        PlaybackService playback(mainWindow.Queue(), library);
        MainController mainController(mainWindow, playback, library);

        app.Run(mainWindow);
    }

    Preferences::SavePluginPreferences();
    LibraryFactory::Instance().Shutdown();

    return 0;
}