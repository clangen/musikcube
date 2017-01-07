#include <pch.h>

#include <win32cpp/Application.hpp>

#include <core/library/LibraryFactory.h>
#include <core/audio/PlaybackService.h>
#include <core/audio/Visualizer.h>

#include <glue/audio/MasterTransport.h>
#include <glue/query/SearchTrackListQuery.h>

#include <app/view/MainWindow.h>

#include <chrono>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::glue;
using namespace musik::glue::audio;
using namespace musik::win;
using namespace win32cpp;

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE previousInstance, LPTSTR commandLine, int showCommand) {
    Application::Initialize(instance, previousInstance, commandLine, showCommand);
    Application& app = Application::Instance();

    MainWindow mainWindow(_TT("musikwin").c_str());
    mainWindow.Initialize();
    mainWindow.Resize(640, 400);
    mainWindow.MoveTo(200, 200);

    LibraryPtr library = LibraryFactory::Libraries().at(0);
    MasterTransport transport;
    PlaybackService playback(mainWindow.Queue(), library, transport);

    std::shared_ptr<SearchTrackListQuery> query =
        std::shared_ptr<SearchTrackListQuery>(new SearchTrackListQuery(library, "o'o"));

    library->Enqueue(query, ILibrary::QuerySynchronous);

    playback.Play(*query->GetResult(), 0);

    //size_t count = vis::VisualizerCount();
    //if (count) {
    //    vis::SetSelectedVisualizer(vis::GetVisualizer(1));
    //    vis::SelectedVisualizer()->Show();
    //}

    app.Run(mainWindow);

    LibraryFactory::Instance().Shutdown();

    return 0;
}