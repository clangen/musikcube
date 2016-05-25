#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>
#include <cursespp/IInput.h>
#include <core/playback/Transport.h>
#include <core/library/LibraryFactory.h>
#include "OutputWindow.h"

namespace musik {
    namespace box {
        class CommandWindow : 
            public cursespp::Window, 
            public cursespp::IInput, 
            public sigslot::has_slots<> 
        {
            public:
                CommandWindow(
                    cursespp::IWindow *parent,
                    musik::core::audio::Transport& transport,
                    musik::core::LibraryPtr library,
                    OutputWindow& output);

                ~CommandWindow();

                virtual void WriteChar(int64 ch);
                virtual void Focus();
                virtual void Show();

            private:
                void ListPlugins() const;
                bool ProcessCommand(const std::string& cmd);
                bool PlayFile(const std::vector<std::string>& args);
                void Pause();
                void Stop();
                void Seek(const std::vector<std::string>& args);
                void SetVolume(const std::vector<std::string>& args);
                void SetVolume(float volume);
                void Help();

                char* buffer;
                int bufferPosition;
                OutputWindow* output;
                musik::core::audio::Transport* transport;
                musik::core::LibraryPtr library;
                bool paused;
        };
    }
}
