//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
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

#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/MessageQueue.h>
#include <cursespp/LayoutStack.h>
#include <cursespp/WindowLayout.h>

#include <app/layout/MainLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/window/OutputWindow.h>
#include <app/util/GlobalHotkeys.h>
#include <app/service/PlaybackService.h>

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#include <core/library/LibraryFactory.h>

#ifdef WIN32
#undef MOUSE_MOVED
#endif

#define IDLE_TIMEOUT_MS 0

struct WindowState {
    ILayoutPtr layout;
    IWindowPtr focused;
    IInput* input;
    IKeyHandler* keyHandler;
};

static void updateFocusedWindow(WindowState& current, IWindowPtr window) {
    current.focused = window;
    current.input = dynamic_cast<IInput*>(window.get());
    current.keyHandler = dynamic_cast<IKeyHandler*>(window.get());
}

static void ensureFocusIsValid(WindowState& current) {
    if (current.layout && current.layout->GetFocus() != current.focused) {
        updateFocusedWindow(current, current.layout->GetFocus());
    }
}

static void changeLayout(WindowState& current, ILayoutPtr newLayout) {
    if (current.layout == newLayout) {
        return;
    }

    if (current.input && current.focused) {
        /* the current input is about to lose focus. reset the timeout */
        wtimeout(current.focused->GetContent(), 0);
    }

    if (current.layout) {
        current.layout->Hide();
        current.layout = { 0 };
    }

    if (newLayout) {
        current.layout = newLayout;
        current.layout->Layout();
        current.layout->Show();
        updateFocusedWindow(current, current.layout->GetFocus());
    }

    if (current.input) {
        curs_set(1);
        wtimeout(current.focused->GetContent(), IDLE_TIMEOUT_MS);
    }
    else {
        curs_set(0);
    }
}

static void focusNextInLayout(WindowState& current) {
    if (!current.layout) {
        return;
    }

    updateFocusedWindow(current, current.layout->FocusNext());

    if (current.input != NULL) {
        curs_set(1);

        if (current.focused) {
            wtimeout(current.focused->GetContent(), IDLE_TIMEOUT_MS);
        }
    }
    else {
        curs_set(0);
    }
}

#ifdef WIN32
int _main(int argc, _TCHAR* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    _main(0, 0);
}

int _main(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    /* the following allows boost::filesystem to use utf8 on Windows */
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);
    
    musik::debug::init();

    ttytype[0] = 30; /* min height */
    ttytype[1] = 30; /* max height */
    ttytype[2] = 120; /* min width */
    ttytype[3] = 120; /* max width */

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    refresh();
    curs_set(0);

#ifdef __PDCURSES__
    PDC_set_title("musikbox ♫");
#endif

    {
        Colors::Init();

        using musik::core::audio::Transport;
        Transport tp;
        tp.SetVolume(0.75);

        PlaybackService playback(tp);

        using musik::core::LibraryFactory;
        LibraryPtr library = LibraryFactory::Libraries().at(0);

        GlobalHotkeys globalHotkeys(tp, library);

        ILayoutPtr libraryLayout(new LibraryLayout(playback, library));
        ILayoutPtr consoleLayout(new MainLayout(tp, library));

        int64 ch;
        timeout(IDLE_TIMEOUT_MS);
        bool quit = false;

        WindowState state = { 0 };
        changeLayout(state, libraryLayout);

        while (!quit) {
            /* if the focused item is an IInput, then get characters from it,
            so it can draw a pretty cursor if it wants */
            if (state.input) {
                WINDOW *c = state.focused->GetContent();
                keypad(c, TRUE);
                ch = wgetch(c);
            }
            else {
                ch = wgetch(stdscr);
            }

            if (ch != -1) { /* -1 = idle timeout */
                std::string kn = keyname((int) ch);

                if (ch == '\t') { /* tab */
                    focusNextInLayout(state);
                }
                else if (kn == "^D") { /* ctrl+d quits */
                    quit = true;
                }
                else if (ch == KEY_F(1)) {
                    changeLayout(state, libraryLayout);
                }
                else if (ch == KEY_F(8)) {
                    changeLayout(state, consoleLayout);
                }
                else if (!globalHotkeys.Handle(ch)) {
                    if (state.input) {
                        state.input->WriteChar(ch);
                    }
                    /* otherwise, send the unhandled keypress directly to the
                    focused window. if it can't do anything with it, send it to
                    the layout for special processing, if necessary */
                    else if (!state.keyHandler || !state.keyHandler->KeyPress(ch)) {
                        state.layout->KeyPress(ch);
                    }
                }
            }

            ensureFocusIsValid(state);
            Window::WriteToScreen();
            MessageQueue::Instance().Dispatch();
        }
    }

    endwin();

    musik::core::LibraryFactory::Instance().Shutdown();
    musik::debug::deinit();

    return 0;
}

