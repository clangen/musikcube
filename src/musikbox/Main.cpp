//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <app/layout/ConsoleLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/IndexerLayout.h>
#include <app/util/GlobalHotkeys.h>
#include <app/service/PlaybackService.h>

#include <boost/locale.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#include <thread>

#include <core/plugin/PluginFactory.h>
#include <core/library/LibraryFactory.h>
#include <core/audio/GaplessTransport.h>

#include <boost/chrono.hpp>
#include <cstdio>

#ifndef WIN32
#include <csignal>
#endif

#ifdef WIN32
#undef MOUSE_MOVED
#endif

#ifdef WIN32
    #define IDLE_TIMEOUT_MS 0
    #define REDRAW_DEBOUNCE_MS 100
#else
    #define IDLE_TIMEOUT_MS 75
    #define REDRAW_DEBOUNCE_MS 100
#endif

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::box;
using namespace cursespp;
using namespace boost::chrono;

static bool disconnected = false;
static int64 resizeAt = 0;

struct WindowState {
    ILayoutPtr layout;
    IWindowPtr focused;
    IInput* input;
    IKeyHandler* keyHandler;
};

static void checkDrawCursor(WindowState& current) {
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

static void updateFocusedWindow(WindowState& current, IWindowPtr window) {
    if (current.focused != window) {
        current.focused = window;
        current.input = dynamic_cast<IInput*>(window.get());
        current.keyHandler = dynamic_cast<IKeyHandler*>(window.get());
        checkDrawCursor(current);
    }
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
        current.layout->BringToTop();
        updateFocusedWindow(current, current.layout->GetFocus());
    }
}

static void focusNextInLayout(WindowState& current) {
    if (!current.layout) {
        return;
    }

    updateFocusedWindow(current, current.layout->FocusNext());
}

static void focusPrevInLayout(WindowState& current) {
    if (!current.layout) {
        return;
    }

    updateFocusedWindow(current, current.layout->FocusPrev());
}

static inline int64 now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

#ifndef WIN32
static void hangupHandler(int signal) {
    disconnected = true;
}

static void resizeHandler(int signal) {
    endwin(); /* required in *nix because? */
    resizeAt = now() + REDRAW_DEBOUNCE_MS;
}
#endif

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

    #if 1 /*DEBUG*/
        freopen("/tmp/musikbox.log", "w", stderr);
    #else
        freopen("/dev/null", "w", stderr);
    #endif

    std::signal(SIGWINCH, resizeHandler);
    std::signal(SIGHUP, hangupHandler);
#endif

#ifdef __PDCURSES__
    PDC_set_resize_limits(12, 60, 60, 250);
    PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, 4);
#endif

    musik::debug::init();
    PluginFactory::Instance(); /* initialize */

    initscr();
    nonl();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();
    curs_set(0);

    {
        Colors::Init();

        LibraryPtr library = LibraryFactory::Libraries().at(0);
        GaplessTransport transport;
        transport.SetVolume(0.75);
        PlaybackService playback(library, transport);

        GlobalHotkeys globalHotkeys(playback, library);

        ILayoutPtr libraryLayout((ILayout *) new LibraryLayout(playback, library));
        ILayoutPtr consoleLayout((ILayout *) new ConsoleLayout(transport, library));
        ILayoutPtr indexerLayout((ILayout *) new IndexerLayout(library));

        int64 ch;
        timeout(IDLE_TIMEOUT_MS);
        bool quit = false;

        WindowState state;
        state.input = NULL;
        state.keyHandler = NULL;

        changeLayout(state, libraryLayout);

        while (!quit && !disconnected) {
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

            if (ch == ERR) {
                std::this_thread::yield();
            }
            else { /* -1 = idle timeout */
                std::string kn = key::Read((int) ch);

                if (ch == '\t') { /* tab */
                    focusNextInLayout(state);
                }
                else if (kn == "KEY_BTAB") { /* shift-tab */
                    focusPrevInLayout(state);
                }
                else if (kn == "^D") { /* ctrl+d quits */
                    quit = true;
                }
                else if (kn == "M-r") {
                    Window::Invalidate();
                }
                else if (kn == "KEY_RESIZE") {
                    resizeAt = now() + REDRAW_DEBOUNCE_MS;
                }
                else if (kn == "M-`" || kn == "M-~") {
                    changeLayout(state, consoleLayout);
                }
                else if (kn == "M-a") {
                    changeLayout(state, libraryLayout);
                }
                else if (kn == "M-s") {
                    changeLayout(state, indexerLayout);
                }
                else if (!globalHotkeys.Handle(kn)) {
                    bool processed = false;
                    if (state.input) {
                        processed = state.input->Write(kn);
                    }

                    /* otherwise, send the unhandled keypress directly to the
                    focused window. if it can't do anything with it, send it to
                    the layout for special processing, if necessary */
                    if (!processed) {
                        if (!state.keyHandler || !state.keyHandler->KeyPress(kn)) {
                            state.layout->KeyPress(kn);
                        }
                    }
                }
            }

            /* KEY_RESIZE often gets called dozens of times, so we debounce the
            actual resize until its settled. */
            if (resizeAt && now() > resizeAt) {
                resize_term(0, 0);
                Window::Invalidate();
                libraryLayout->Layout();
                consoleLayout->Layout();
                indexerLayout->Layout();
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
