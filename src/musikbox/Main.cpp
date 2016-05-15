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

#include "Colors.h"
#include "LogWindow.h"
#include "CommandWindow.h"
#include "OutputWindow.h"
#include "TransportWindow.h"
#include "ResourcesWindow.h"
#include "MainLayout.h"
#include "LibraryLayout.h"
#include "IInput.h"

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#include <core/library/LibraryFactory.h>

#ifdef WIN32
#undef MOUSE_MOVED

#define IDLE_TIMEOUT_MS 0

struct WindowState {
    ILayout* layout;
    IWindow* focused;
    IInput* input;
    IScrollable* scrollable;
};

void changeLayout(WindowState& current, ILayout* newLayout) {
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
        current.focused = current.layout->GetFocus();
        current.input = dynamic_cast<IInput*>(current.focused);
        current.scrollable = dynamic_cast<IScrollable*>(current.focused);
    }

    if (current.input) {
        curs_set(1);
        wtimeout(current.focused->GetContent(), IDLE_TIMEOUT_MS);
    }
    else {
        curs_set(0);
    }
}

void focusNextInLayout(WindowState& current) {
    if (!current.layout) {
        return;
    }

    current.focused = current.layout->FocusNext();
    current.scrollable = dynamic_cast<IScrollable*>(current.focused);
    current.input = dynamic_cast<IInput*>(current.focused);

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
        tp.SetVolume(0.01);

        using musik::core::LibraryFactory;
        LibraryPtr library = LibraryFactory::Libraries().at(0);

        MainLayout mainLayout(tp, library);
        LibraryLayout libraryLayout(library);

        mainLayout.Hide();
        libraryLayout.Hide();

        int64 ch;
        timeout(IDLE_TIMEOUT_MS);
        bool quit = false;

        WindowState state = { 0 };
        changeLayout(state, &mainLayout);

        while (!quit) {
            /* if the focused item is an IInput, then get characters from it,
            so it can draw a pretty cursor if it wants */
            if (state.input) {
                WINDOW *c = state.focused->GetContent();
                keypad(c, TRUE);
                ch = wgetch(c);
            }
            else {
                ch = getch();
            }
            if (ch == -1) { /* timeout */
                state.layout->OnIdle();
            }
            else if (ch == 9) { /* tab */
                focusNextInLayout(state);
            }
            else if (ch >= KEY_F(0) && ch <= KEY_F(12)) {
                if (ch == KEY_F(1)) {
                    changeLayout(state, &mainLayout);
                }
                else if (ch == KEY_F(2)) {
                    changeLayout(state, &libraryLayout);
                }
            }
            else if (ch == KEY_NPAGE) {
                if (state.scrollable) {
                    state.scrollable->PageDown();
                }
            }
            else if (ch == KEY_PPAGE) {
                if (state.scrollable) {
                    state.scrollable->PageUp();
                }
            }
            else if (ch == KEY_DOWN) {
                if (state.scrollable) {
                    state.scrollable->ScrollDown();
                }
            }
            else if (ch == KEY_UP) {
                if (state.scrollable) {
                    state.scrollable->ScrollUp();
                }
            }
            else if (ch == KEY_HOME) {
                if (state.scrollable) {
                    state.scrollable->ScrollToTop();
                }
            }
            else if (ch == KEY_END) {
                if (state.scrollable) {
                    state.scrollable->ScrollToBottom();
                }
            }
            else if (state.input) {
                state.input->WriteChar(ch);
            }

            Window::WriteToScreen();
        }
    }

    endwin();

    musik::core::LibraryFactory::Instance().Shutdown();
    musik::debug::deinit();

    return 0;
}

