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

#include <stdafx.h>

#include "App.h"
#include "Colors.h"
#include "ILayout.h"
#include "IInput.h"
#include "Window.h"
#include "MessageQueue.h"
#include "Text.h"

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/chrono.hpp>

#include <thread>

#ifndef WIN32
#include <csignal>
#endif

#ifdef WIN32
#define IDLE_TIMEOUT_MS 0
#define REDRAW_DEBOUNCE_MS 100
#else
#define IDLE_TIMEOUT_MS 75
#define REDRAW_DEBOUNCE_MS 100
#endif

using namespace cursespp;
using namespace boost::chrono;

static bool disconnected = false;
static int64 resizeAt = 0;

#ifndef WIN32
static void hangupHandler(int signal) {
    disconnected = true;
}

static void resizedHandler(int signal) {
    endwin(); /* required in *nix because? */
    resizeAt = App::Now() + REDRAW_DEBOUNCE_MS;
}
#endif

App::App(const std::string& title) {
    /* the following allows boost::filesystem to use utf8 on Windows */
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);

#ifndef WIN32
    setlocale(LC_ALL, "");
    std::signal(SIGWINCH, resizedHandler);
    std::signal(SIGHUP, hangupHandler);
#endif

#ifdef __PDCURSES__
    PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, 4);
#endif

    initscr();
    nonl();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();
    curs_set(0);

#ifndef WIN32
    set_escdelay(50);
#endif

#ifdef __PDCURSES__
    PDC_set_title(title.c_str());
#endif

    Colors::Init();
}

App::~App() {
}

void App::SetKeyHandler(MainKeyHandler handler) {
    this->keyHandler = handler;
}

void App::SetResizeHandler(ResizeHandler handler) {
    this->resizeHandler = handler;
}

void App::Run(ILayoutPtr layout) {
    int64 ch;
    timeout(IDLE_TIMEOUT_MS);
    bool quit = false;

    this->state.input = nullptr;
    this->state.keyHandler = nullptr;

    this->ChangeLayout(layout);

    while (!quit && !disconnected) {
        /* if the focused item is an IInput, then get characters from it,
        so it can draw a pretty cursor if it wants */
        if (this->state.input) {
            WINDOW *c = this->state.focused->GetContent();
            wtimeout(this->state.focused->GetContent(), IDLE_TIMEOUT_MS);
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
        else {
            std::string kn = key::Read((int) ch);

            if (ch == '\t') { /* tab */
                this->FocusNextInLayout();
            }
            else if (kn == "KEY_BTAB") { /* shift-tab */
                this->FocusPrevInLayout();
            }
            else if (kn == "^D") { /* ctrl+d quits */
                quit = true;
            }
            else if (kn == "M-r") {
                Window::Invalidate();
            }
            else if (kn == "KEY_RESIZE") {
                resizeAt = App::Now() + REDRAW_DEBOUNCE_MS;
            }
            /* order: focused input, global key handler, then layout. */
            else if (!this->state.input || !this->state.input->Write(kn)) {
                if (!keyHandler || !keyHandler(kn)) {
                    if (!this->state.keyHandler || !this->state.keyHandler->KeyPress(kn)) {
                        this->state.layout->KeyPress(kn);
                    }
                }
            }
        }

        /* KEY_RESIZE often gets called dozens of times, so we debounce the
        actual resize until its settled. */
        if (resizeAt && App::Now() > resizeAt) {
            resize_term(0, 0);
            Window::Invalidate();

            if (this->resizeHandler) {
                this->resizeHandler();
            }

            this->state.layout->BringToTop();
            resizeAt = 0;
        }

        this->EnsureFocusIsValid();
        Window::WriteToScreen(this->state.input);
        MessageQueue::Instance().Dispatch();
    }
}

void App::CheckDrawCursor() {
    if (this->state.input != NULL) {
        curs_set(1);

        if (this->state.focused) {
            wtimeout(this->state.focused->GetContent(), IDLE_TIMEOUT_MS);
        }
    }
    else {
        curs_set(0);
    }
}

void App::UpdateFocusedWindow(IWindowPtr window) {
    if (this->state.focused != window) {
        this->state.focused = window;
        this->state.input = dynamic_cast<IInput*>(window.get());
        this->state.keyHandler = dynamic_cast<IKeyHandler*>(window.get());
        this->CheckDrawCursor();
    }
}

void App::EnsureFocusIsValid() {
    if (this->state.layout && this->state.layout->GetFocus() != this->state.focused) {
        this->UpdateFocusedWindow(this->state.layout->GetFocus());
    }
}

void App::ChangeLayout(ILayoutPtr newLayout) {
    if (this->state.layout == newLayout) {
        return;
    }

    if (this->state.input && this->state.focused) {
        /* the current input is about to lose focus. reset the timeout */
        wtimeout(this->state.focused->GetContent(), 0);
    }

    if (this->state.layout) {
        this->state.layout->Hide();
        this->state.layout.reset();
    }

    if (newLayout) {
        this->state.layout = newLayout;
        this->state.layout->Layout();
        this->state.layout->Show();
        this->state.layout->BringToTop();
        this->UpdateFocusedWindow(this->state.layout->GetFocus());
    }
}

void App::FocusNextInLayout() {
    if (!this->state.layout) {
        return;
    }

    this->UpdateFocusedWindow(this->state.layout->FocusNext());
}

void App::FocusPrevInLayout() {
    if (!this->state.layout) {
        return;
    }

    this->UpdateFocusedWindow(this->state.layout->FocusPrev());
}

int64 App::Now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}