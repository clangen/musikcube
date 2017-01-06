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
#include "Text.h"
#include "Screen.h"

#include <thread>

#ifdef WIN32
#include <app/util/Win32Util.h>
#endif

#ifndef WIN32
#include <csignal>
#endif

#define HIDDEN_IDLE_TIMEOUT_MS 500

using namespace cursespp;
using namespace std::chrono;

static OverlayStack overlays;
static bool disconnected = false;
static int64 resizeAt = 0;

static inline bool isVisible() {
#ifdef WIN32
    static HWND hwnd = nullptr;
    if (!hwnd) {
        hwnd = musik::box::win32::GetMainWindow();
    }
    if (hwnd) {
        return !IsIconic(hwnd);
    }
    return true;
#else
    return true;
#endif
}

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
    this->minWidth = this->minHeight = 0;

#ifndef WIN32
    setlocale(LC_ALL, "");
    std::signal(SIGWINCH, resizedHandler);
    std::signal(SIGHUP, hangupHandler);
    std::signal(SIGPIPE, SIG_IGN);
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
}

App::~App() {
    endwin();
}

void App::SetKeyHandler(MainKeyHandler handler) {
    this->keyHandler = handler;
}

void App::SetResizeHandler(ResizeHandler handler) {
    this->resizeHandler = handler;
}

void App::SetCustomColorsDisabled(bool disabled) {
    this->disableCustomColors = disabled;
}

void App::SetMinimumSize(int minWidth, int minHeight) {
    this->minWidth = std::max(0, minWidth);
    this->minHeight = std::max(0, minHeight);
}

void App::OnResized() {
    int cx = Screen::GetWidth();
    int cy = Screen::GetHeight();
    if (cx < this->minWidth || cy < this->minHeight) {
        Window::Freeze();
    }
    else {
        Window::Unfreeze();

        if (this->state.layout) {
            if (this->state.viewRoot) {
                this->state.viewRoot->ResizeToViewport();
            }

            this->state.layout->Layout();
            this->state.layout->BringToTop();
        }

        if (this->state.overlay) {
            this->state.overlay->Layout();
            this->state.overlay->BringToTop();
        }
    }
}

void App::Run(ILayoutPtr layout) {
    Colors::Init(this->disableCustomColors);

    int64 ch;

    bool quit = false;

    bool wasVisible = true;
    bool visible = true;

    this->state.input = nullptr;
    this->state.keyHandler = nullptr;

    this->ChangeLayout(layout);

    while (!quit && !disconnected) {
        visible = isVisible();

        timeout(visible
            ? IDLE_TIMEOUT_MS
            : HIDDEN_IDLE_TIMEOUT_MS);

        if (this->state.input) {
            /* if the focused window is an input, allow it to draw a cursor */
            WINDOW *c = this->state.focused->GetContent();
            keypad(c, TRUE);
            ch = wgetch(c);
        }
        else {
            /* otherwise, no cursor */
            ch = wgetch(stdscr);
        }

        if (ch != ERR) {
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
                Window::InvalidateScreen();
            }
            else if (kn == "KEY_RESIZE") {
                resizeAt = App::Now() + REDRAW_DEBOUNCE_MS;
            }
            /* order: focused input, global key handler, then layout. */
            else if (!this->state.input ||
                !this->state.focused->IsVisible() ||
                !this->state.input->Write(kn))
            {
                if (!keyHandler || !keyHandler(kn)) {
                    if (!this->state.keyHandler || !this->state.keyHandler->KeyPress(kn)) {
                        this->state.ActiveLayout()->KeyPress(kn);
                    }
                }
            }
        }

        /* KEY_RESIZE often gets called dozens of times, so we debounce the
        actual resize until its settled. */
        if (resizeAt && App::Now() > resizeAt) {
            resize_term(0, 0);

            Window::InvalidateScreen();

            if (this->resizeHandler) {
                this->resizeHandler();
            }

            this->OnResized();

            resizeAt = 0;
        }

        if (visible || !visible && wasVisible) {
            this->CheckShowOverlay();
            this->EnsureFocusIsValid();
            Window::WriteToScreen(this->state.input);
        }

        Window::MessageQueue().Dispatch();

        wasVisible = visible;
    }

    overlays.Clear();
}

void App::UpdateFocusedWindow(IWindowPtr window) {
    if (this->state.focused != window) {
        this->state.focused = window;
        this->state.input = dynamic_cast<IInput*>(window.get());
        this->state.keyHandler = dynamic_cast<IKeyHandler*>(window.get());
    }
}

void App::EnsureFocusIsValid() {
    ILayoutPtr layout = this->state.ActiveLayout();
    if (layout && layout->GetFocus() != this->state.focused) {
        this->UpdateFocusedWindow(this->state.ActiveLayout()->GetFocus());
    }
}

OverlayStack& App::Overlays() {
    return overlays;
}

void App::CheckShowOverlay() {
    ILayoutPtr top = overlays.Top();

    if (top != this->state.overlay) {
        if (this->state.overlay) {
            this->state.overlay->Hide();
        }

        this->state.overlay = top;

        ILayoutPtr newTopLayout = this->state.ActiveLayout();
        if (newTopLayout) {
            newTopLayout->Layout();
            newTopLayout->Show();
            newTopLayout->BringToTop();
        }
    }
}

void App::ChangeLayout(ILayoutPtr newLayout) {
    if (this->state.layout == newLayout) {
        return;
    }

    if (this->state.input && this->state.focused) {
        /* the current input is about to lose focus. reset the timeout */
        wtimeout(
            this->state.focused->GetContent(),
            isVisible() ? 0 : HIDDEN_IDLE_TIMEOUT_MS);
    }

    if (this->state.layout) {
        this->state.layout->Hide();
        this->state.layout.reset();
    }

    if (newLayout) {
        this->state.layout = newLayout;
        this->state.viewRoot = dynamic_cast<IViewRoot*>(this->state.layout.get());

        if (this->state.viewRoot) {
            this->state.viewRoot->ResizeToViewport();
        }

        this->state.layout->Show();
        this->state.layout->BringToTop();

        if (!this->state.overlay) {
            this->UpdateFocusedWindow(this->state.layout->GetFocus());
        }
    }

    this->CheckShowOverlay();
}

void App::FocusNextInLayout() {
    if (!this->state.ActiveLayout()) {
        return;
    }

    this->UpdateFocusedWindow(this->state.ActiveLayout()->FocusNext());
}

void App::FocusPrevInLayout() {
    if (!this->state.ActiveLayout()) {
        return;
    }

    this->UpdateFocusedWindow(this->state.ActiveLayout()->FocusPrev());
}

int64 App::Now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}