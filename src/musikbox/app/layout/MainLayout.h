#pragma once

#include <cursespp/LayoutBase.h>

#include <app/window/LogWindow.h>
#include <app/window/CommandWindow.h>
#include <app/window/OutputWindow.h>
#include <app/window/TransportWindow.h>
#include <app/window/ResourcesWindow.h>

#include <vector>

#include <core/playback/Transport.h>
#include <boost/shared_ptr.hpp>

using musik::core::audio::Transport;

class MainLayout : public LayoutBase {
    public:
        MainLayout(Transport& transport, LibraryPtr library);
        ~MainLayout();

        virtual void Layout();
        virtual void Show();
        virtual void ProcessMessage(IWindowMessage &message);

    private:
        void UpdateWindows();

        std::shared_ptr<LogWindow> logs;
        std::shared_ptr<CommandWindow> commands;
        std::shared_ptr<OutputWindow> output;
        std::shared_ptr<ResourcesWindow> resources;
};