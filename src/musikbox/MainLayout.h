#pragma once

#include "LayoutBase.h"
#include "LogWindow.h"
#include "CommandWindow.h"
#include "OutputWindow.h"
#include "TransportWindow.h"
#include "ResourcesWindow.h"

#include <vector>

#include <core/playback/Transport.h>
#include <boost/shared_ptr.hpp>

using musik::core::audio::Transport;

class MainLayout : public LayoutBase {
    public:
        MainLayout(Transport& transport, LibraryPtr library);
        ~MainLayout();

        virtual void Layout();
        virtual void OnIdle();
        virtual void Show();

    private:
        void UpdateWindows();

        std::shared_ptr<LogWindow> logs;
        std::shared_ptr<CommandWindow> commands;
        std::shared_ptr<OutputWindow> output;
        std::shared_ptr<TransportWindow> transport;
        std::shared_ptr<ResourcesWindow> resources;
};