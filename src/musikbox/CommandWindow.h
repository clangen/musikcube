#pragma once

#include "curses_config.h"
#include "BorderedWindow.h"
#include "OutputWindow.h"
#include "IInput.h"
#include <core/playback/Transport.h>
#include <core/library/LibraryFactory.h>

using musik::core::LibraryPtr;
using namespace musik::core::audio;

class CommandWindow : public BorderedWindow, public IInput {
    public:
        CommandWindow(Transport& transport, OutputWindow& output);
        ~CommandWindow();

        virtual void WriteChar(int ch);

    private:
        void ListPlugins() const;
        bool ProcessCommand(const std::string& cmd);
        bool PlayFile(const std::vector<std::string>& args);
        void Pause();
        void Stop();
        void ListPlaying();
        void Seek(const std::vector<std::string>& args);
        void SetVolume(const std::vector<std::string>& args);
        void SetVolume(float volume);
        void Help();

        char* buffer;
        int bufferPosition;
        OutputWindow* output;
        Transport* transport;
        LibraryPtr library;
        bool paused;
};