//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include "Win32Util.h"
#endif

#ifndef WIN32
#include <csignal>
#endif

using namespace cursespp;
using namespace std::chrono;

static OverlayStack overlays;
static bool disconnected = false;
static int64_t resizeAt = 0;

static App* instance = nullptr;

#ifndef WIN32
static void hangupHandler(int signal) {
    disconnected = true;
}

static void resizedHandler(int signal) {
    endwin(); /* required in *nix because? */
    resizeAt = App::Now() + REDRAW_DEBOUNCE_MS;
}
#endif

App& App::Instance() {
    if (!instance) {
        throw std::runtime_error("app not running!");
    }
    return *instance;
}

App::App(const std::string& title) {
    if (instance) {
        throw std::runtime_error("app instance already running!");
    }

    instance = this; /* only one instance. */

    this->quit = false;
    this->minWidth = this->minHeight = 0;
    this->mouseEnabled = true;

#ifdef WIN32
    this->iconId = 0;
    this->appTitle = title;
    win32::ConfigureDpiAwareness();
#else
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
    mousemask(ALL_MOUSE_EVENTS, nullptr);

#ifndef WIN32
    set_escdelay(20);
#endif

#ifdef __PDCURSES__
    PDC_set_title(title.c_str());
    win32::InterceptWndProc();
    win32::SetAppTitle(title);
#endif
}

App::~App() {
    endwin();
}

void App::SetKeyHandler(KeyHandler handler) {
    this->keyHandler = handler;
}

void App::SetKeyHook(KeyHandler hook) {
    this->keyHook = hook;
}

void App::SetResizeHandler(ResizeHandler handler) {
    this->resizeHandler = handler;
}

void App::SetColorMode(Colors::Mode mode) {
    this->colorMode = mode;
}

void App::SetColorBackgroundType(Colors::BgType bgType) {
    this->bgType = bgType;
}

void App::SetColorTheme(const std::string& colorTheme) {
    this->colorTheme = colorTheme;
}

void App::SetMinimumSize(int minWidth, int minHeight) {
    this->minWidth = std::max(0, minWidth);
    this->minHeight = std::max(0, minHeight);
}

#ifdef WIN32
void App::SetIcon(int resourceId) {
    this->iconId = resourceId;
    if (win32::GetMainWindow()) {
        win32::SetIcon(resourceId);
    }
}

void App::SetSingleInstanceId(const std::string& uniqueId) {
    this->uniqueId = uniqueId;
}

bool App::RegisterFont(const std::string& filename) {
    return win32::RegisterFont(filename) > 0;
}

void App::SetDefaultFontface(const std::string& fontface) {
    PDC_set_preferred_fontface(u8to16(fontface).c_str());
}

void App::SetDefaultMenuVisibility(bool visible) {
    PDC_set_default_menu_visibility(visible);
}
#endif

void App::SetMinimizeToTray(bool minimizeToTray) {
#ifdef WIN32
    win32::SetMinimizeToTray(minimizeToTray);
#endif
}

void App::Minimize() {
#ifdef WIN32
    win32::Minimize();
#endif
}

void App::Restore() {
#ifdef WIN32
    win32::ShowMainWindow();
#endif
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

void App::InjectKeyPress(const std::string& key) {
    this->injectedKeys.push(key);
}

void App::Quit() {
    this->quit = true;
}

void App::SetMouseEnabled(bool enabled) {
    this->mouseEnabled = enabled;
}

#ifdef WIN32
bool App::Running(const std::string& uniqueId) {
    return App::Running(uniqueId, uniqueId);
}

bool App::Running(const std::string& uniqueId, const std::string& title) {
    if (uniqueId.size()) {
        win32::EnableSingleInstance(uniqueId);
        if (win32::AlreadyRunning()) {
            win32::ShowOtherInstance(title);
            return true;
        }
    }
    return false;
}
#endif

void App::Run(ILayoutPtr layout) {
#ifdef WIN32
    if (App::Running(this->uniqueId, this->appTitle)) {
        return;
    }
#endif

    Colors::Init(this->colorMode, this->bgType);

    if (this->colorTheme.size()) {
        Colors::SetTheme(this->colorTheme);
    }

    MEVENT mouseEvent;
    int64_t ch;
    std::string kn;

    this->state.input = nullptr;
    this->state.keyHandler = nullptr;

    this->ChangeLayout(layout);

    while (!this->quit && !disconnected) {
        kn = "";

        if (this->injectedKeys.size()) {
            kn = injectedKeys.front();
            injectedKeys.pop();
            ch = ERR;
            goto process;
        }

        timeout(IDLE_TIMEOUT_MS);

        if (this->state.input) {
            /* if the focused window is an input, allow it to draw a cursor */
            WINDOW *c = this->state.focused->GetContent();
            keypad(c, TRUE);
            wtimeout(c, IDLE_TIMEOUT_MS);
            ch = wgetch(c);
        }
        else {
            /* otherwise, no cursor */
            ch = wgetch(stdscr);
        }

        if (ch != ERR) {
            kn = key::Read((int) ch);

            if (this->keyHook) {
                if (this->keyHook(kn)) {
                    continue;
                }
            }

process:
            if (ch == '\t') { /* tab */
                this->FocusNextInLayout();
            }
            else if (kn == "KEY_BTAB") { /* shift-tab */
                this->FocusPrevInLayout();
            }
            else if (kn == "^D") { /* ctrl+d quits */
                this->quit = true;
            }
            else if (kn == "KEY_RESIZE") {
                resizeAt = App::Now() + REDRAW_DEBOUNCE_MS;
            }
            else if (this->mouseEnabled && kn == "KEY_MOUSE") {
#ifdef WIN32
                if (nc_getmouse(&mouseEvent) == 0) {
#else
                if (getmouse(&mouseEvent) == 0) {
#endif
                    auto active = this->state.ActiveLayout();
                    if (active) {
                        using Event = IMouseHandler::Event;
                        auto window = dynamic_cast<IWindow*>(active.get());
                        Event event(mouseEvent, window);
                        if (event.MouseWheelDown() || event.MouseWheelUp()) {
                            if (state.focused) {
                                state.focused->MouseEvent(event);
                            }
                        }
                        else {
                            active->MouseEvent(event);
                        }
                    }
                }
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

        this->CheckShowOverlay();
        this->EnsureFocusIsValid();

        /* needs to happen here, or else flicker */
        Window::MessageQueue().Dispatch();

        if (Window::WriteToScreen(this->state.input)) {
            if (this->state.overlayWindow && !this->state.overlayWindow->IsTop()) {
                this->state.overlay->BringToTop(); /* active overlay is always on top... */
            }
        }
    }

    overlays.Clear();
}

void App::UpdateFocusedWindow(IWindowPtr window) {
    if (this->state.focused != window) {
        if (this->state.focused) {
            this->state.focused->Blur();
        }

        this->state.focused = window;
        this->state.input = dynamic_cast<IInput*>(window.get());
        this->state.keyHandler = dynamic_cast<IKeyHandler*>(window.get());

        if (this->state.focused) {
            this->state.focused->Focus();
        }
    }
}

void App::EnsureFocusIsValid() {
    ILayoutPtr layout = this->state.ActiveLayout();
    if (layout) {
        IWindowPtr focused = layout->GetFocus();
        if (focused != this->state.focused) {
            this->UpdateFocusedWindow(focused);
            Window::InvalidateScreen();
        }
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

        this->state.overlayWindow =
            top ? dynamic_cast<IWindow*>(top.get()) : nullptr;

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
        wtimeout(this->state.focused->GetContent(), IDLE_TIMEOUT_MS);
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

int64_t App::Now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}
