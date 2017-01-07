#include <pch.h>

#include <win32cpp/Application.hpp>
#include <win32cpp/TopLevelWindow.hpp>

#include <core/library/LibraryFactory.h>
#include <glue/audio/MasterTransport.h>

using namespace musik::core;
using namespace musik::glue::audio;
using namespace win32cpp;

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand) {
    LibraryPtr library = LibraryFactory::Libraries().at(0);
    MasterTransport transport;

    transport.Start("c:\\test.ogg");

    Application::Initialize(instance, previousInstance, commandLine, showCommand);
    Application& app = Application::Instance();

    TopLevelWindow mainWindow(_TT("musikwin").c_str());
    mainWindow.Resize(800, 600);
    mainWindow.MoveTo(10, 10);

    app.Run(mainWindow);

    LibraryFactory::Instance().Shutdown();

    return 0;
}