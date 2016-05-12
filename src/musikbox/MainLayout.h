#pragma once

#include "ILayout.h"
#include "LogWindow.h"
#include "CommandWindow.h"
#include "OutputWindow.h"
#include "TransportWindow.h"
#include "ResourcesWindow.h"

#include <vector>

#include <core/playback/Transport.h>
#include <boost/shared_ptr.hpp>

using musik::core::audio::Transport;

class MainLayout : public ILayout {
    public:
        MainLayout(Transport& transport, LibraryPtr library);
        ~MainLayout();

        virtual IWindow* FocusNext();
        virtual IWindow* FocusPrev();
        virtual IWindow* GetFocus();
        virtual void Layout();
        virtual void OnIdle();

    private:
        boost::shared_ptr<LogWindow> logs;
        boost::shared_ptr<CommandWindow> commands;
        boost::shared_ptr<OutputWindow> output;
        boost::shared_ptr<TransportWindow> transport;
        boost::shared_ptr<ResourcesWindow> resources;
        
        std::vector<IWindow*> focusOrder;
        int focusIndex;
};