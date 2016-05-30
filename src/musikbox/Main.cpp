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

#include <core/plugin/PluginFactory.h>
#include <core/library/LibraryFactory.h>

#include <boost/chrono.hpp>

#include <cstdio>

#ifdef WIN32
#undef MOUSE_MOVED
#endif

#ifdef WIN32
    #define IDLE_TIMEOUT_MS 0
    #define REDRAW_DEBOUNCE_MS 100
#else
    #define IDLE_TIMEOUT_MS 100
    #define REDRAW_DEBOUNCE_MS 200
#endif

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;
using namespace boost::chrono;

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
        current.layout.reset();
    }

    if (newLayout) {
        current.layout = newLayout;
        current.layout->Layout();
        current.layout->Show();
        updateFocusedWindow(current, current.layout->GetFocus());
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

static inline std::string readKeyPress(int64 ch) {
    std::string kn = keyname((int)ch);

    /* convert +ESC to M- sequences */
    if (kn == "^[") {
        int64 next = getch();
        if (next != -1) {
            kn = std::string("M-") + std::string(keyname((int)next));
        }
    }
    /* multi-byte UTF8 character */
    else if (ch >= 194 && ch <= 223) {
        kn = "";
        kn += (char)ch;
        kn += (char)getch();
    }
    else if (ch >= 224 && ch <= 239) {
        kn = "";
        kn += (char)ch;
        kn += (char)getch();
        kn += (char)getch();
    }
    else if (ch >= 240 && ch <= 244) {
        kn = "";
        kn += (char)ch;
        kn += (char)getch();
        kn += (char)getch();
        kn += (char)getch();
    }


    //std::cerr << "keyname: " << kn << std::endl;
    // std::cerr << "ch: " << ch << std::endl;

    return kn;
}

static inline int64 now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
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

#ifndef WIN32
    setlocale(LC_ALL, "");

    #ifdef DEBUG
        freopen("/tmp/musikbox.log", "w", stderr);
    #else
        freopen("/dev/null", "w", stderr);
    #endif
#endif

    musik::debug::init();
    PluginFactory::Instance(); /* initialize */

#ifdef __PDCURSES__
    PDC_set_resize_limits(26, 38, 100, 150);
    PDC_set_title("musikbox ♫");
    PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, 4);
#endif

    initscr();
    nonl();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    refresh();
    curs_set(0);

    {
        Colors::Init();

        Transport tp;
        tp.SetVolume(0.75);

        PlaybackService playback(tp);

        LibraryPtr library = LibraryFactory::Libraries().at(0);

        GlobalHotkeys globalHotkeys(playback, library);

        ILayoutPtr libraryLayout((ILayout *) new LibraryLayout(playback, library));
        ILayoutPtr consoleLayout((ILayout *) new MainLayout(tp, library));

        int64 ch;
        timeout(IDLE_TIMEOUT_MS);
        bool quit = false;
        int64 resizeAt = 0;

        WindowState state;
        state.input = NULL;
        state.keyHandler = NULL;

        changeLayout(state, consoleLayout);

        while (!quit) {
            /* if the focused item is an IInput, then get characters from it,
            so it can draw a pretty cursor if it wants */
            if (state.input) {
                WINDOW *c = state.focused->GetContent();
                wtimeout(state.focused->GetContent(), IDLE_TIMEOUT_MS);
                curs_set(1);
                keypad(c, TRUE);
                ch = wgetch(c);
            }
            else {
                ch = wgetch(stdscr);
                curs_set(0);
            }

            if (ch != -1) { /* -1 = idle timeout */
                std::string kn = readKeyPress((int) ch);

                if (ch == '\t') { /* tab */
                    focusNextInLayout(state);
                }
                else if (kn == "^D") { /* ctrl+d quits */
                    quit = true;
                }
                else if (kn == "ALT_R" || kn == "M-r") {
                    Window::Invalidate();
                }
                else if (kn == "KEY_RESIZE") {
                    resizeAt = now() + REDRAW_DEBOUNCE_MS;
                }
                else if (ch == KEY_F(2)) {
                    changeLayout(state, libraryLayout);
                }
                else if (ch == KEY_F(1)) {
                    changeLayout(state, consoleLayout);
                }
                else if (!globalHotkeys.Handle(kn)) {
                    if (state.input) {
                        state.input->Write(kn);
                    }
                    /* otherwise, send the unhandled keypress directly to the
                    focused window. if it can't do anything with it, send it to
                    the layout for special processing, if necessary */
                    else if (!state.keyHandler || !state.keyHandler->KeyPress(kn)) {
                        state.layout->KeyPress(kn);
                    }
                }
            }

            /* this is a bit of a hack, and if we have any more of these we
            need to generalize. but KEY_RESIZE often gets called dozens of
            times, so we debounce the actual resize until its settled. */
            if (resizeAt && now() > resizeAt) {
                resize_term(0, 0);
                libraryLayout->Layout();
                consoleLayout->Layout();
                state.layout->BringToTop();
                resizeAt = 0;
            }

            ensureFocusIsValid(state);
            Window::WriteToScreen(state.input);
            MessageQueue::Instance().Dispatch();
        }

        playback.Stop();
    }

    endwin();

    musik::core::LibraryFactory::Instance().Shutdown();
    musik::debug::deinit();

    return 0;
}
