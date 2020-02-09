//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/curses_config.h>
#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/ILayout.h>
#include <cursespp/IInput.h>
#include <cursespp/Window.h>
#include <cursespp/Text.h>
#include <cursespp/Screen.h>
#include <algorithm>
#include <thread>
#include <iostream>

#ifdef WIN32
#include <cursespp/Win32Util.h>
#undef MOUSE_MOVED
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#ifndef WIN32
#include <csignal>
#include <cstdlib>
#include <locale.h>
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

static bool isLangUtf8() {
    const char* lang = std::getenv("LANG");

    if (!lang) {
        return false;
    }

    std::string str = std::string(lang);
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    return
        str.find("utf-8") != std::string::npos ||
        str.find("utf8") != std::string::npos;
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

    this->SetTitle(title);

#ifdef WIN32
    this->iconId = 0;
    this->colorMode = Colors::RGB;
    win32::ConfigureDpiAwareness();
#else
    setlocale(LC_ALL, "");
    std::signal(SIGWINCH, resizedHandler);
    std::signal(SIGHUP, hangupHandler);
    std::signal(SIGPIPE, SIG_IGN);
    this->colorMode = Colors::Palette;
#endif
}

App::~App() {
    endwin();
}

void App::InitCurses() {
#ifdef WIN32
    PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, 4);
    #ifdef PDCURSES_WINGUI
        PDC_set_default_menu_visibility(0);
        PDC_set_title(this->appTitle.c_str());
        PDC_set_color_intensify_enabled(false);
    #endif
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

#ifdef PDCURSES_WINGUI
    /* needs to happen after initscr() */
    win32::InterceptWndProc();
    win32::SetAppTitle(this->appTitle);
    if (this->iconId) {
        this->SetIcon(this->iconId);
    }
#endif

    Colors::Init(this->colorMode, this->bgType);

    if (this->colorTheme.size()) {
        Colors::SetTheme(this->colorTheme);
    }

    this->initialized = true;
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
#if defined(PDCURSES_WINCON)
    mode = Colors::Basic;
#endif
    this->colorMode = mode;
    if (this->initialized) {
        Colors::Init(this->colorMode, this->bgType);
    }
}

void App::SetColorBackgroundType(Colors::BgType bgType) {
    this->bgType = bgType;

    if (this->initialized) {
        Colors::Init(this->colorMode, this->bgType);
    }
}

void App::SetColorTheme(const std::string& colorTheme) {
    this->colorTheme = colorTheme;

    if (this->initialized) {
        Colors::SetTheme(colorTheme);
    }
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
#if defined(PDCURSES_WINGUI)
    return win32::RegisterFont(filename) > 0;
#else
    return false;
#endif
}

void App::SetDefaultFontface(const std::string& fontface) {
#if defined(PDCURSES_WINGUI)
    PDC_set_preferred_fontface(u8to16(fontface).c_str());
#endif
}

void App::SetDefaultMenuVisibility(bool visible) {
#if defined(PDCURSES_WINGUI)
    PDC_set_default_menu_visibility(visible);
#endif
}
#endif

void App::SetTitle(const std::string& title) {
    this->appTitle = title;
#ifdef WIN32
    PDC_set_title(this->appTitle.c_str());
    win32::SetAppTitle(this->appTitle);
#else
    /* stolen from https://github.com/cmus/cmus/blob/fead80b207b79ae6d10ab2b1601b11595d719908/ui_curses.c#L2349 */
    const char* term = getenv("TERM");
    if (term) {
        if (!strcmp(term, "screen")) {
            std::cout << "\033_" << this->appTitle.c_str() << "\033\\";
        }
        else if (!strncmp(term, "xterm", 5) ||
                 !strncmp(term, "rxvt", 4) ||
                 !strcmp(term, "Eterm"))
        {
            std::cout << "\033]0;" << this->appTitle.c_str() << "\007";
        }
    }
    Window::InvalidateScreen();
#endif
}

void App::SetMinimizeToTray(bool minimizeToTray) {
#ifdef PDCURSES_WINGUI
    win32::SetMinimizeToTray(minimizeToTray);
#endif
}

void App::Minimize() {
#ifdef PDCURSES_WINGUI
    win32::Minimize();
#endif
}

void App::Restore() {
#ifdef PDCURSES_WINGUI
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
            if (this->state.rootWindow) {
                this->state.rootWindow->MoveAndResize(
                    0, 0, Screen::GetWidth(), Screen::GetHeight());
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

void App::SetQuitKey(const std::string& kn) {
    this->quitKey = kn;
}

std::string App::GetQuitKey() {
    return this->quitKey;
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
#else
    if (!isLangUtf8()) {
        std::cout << "\n\nThis application requires a UTF-8 compatible LANG environment "
        "variable to be set in the controlling terminal. Exiting.\n\n\n";
        return;
    }
#endif

    this->InitCurses();

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
            else if (kn == this->quitKey) { /* ctrl+d quits */
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

        /* we want to dispatch messages here, before we call WriteToScreen(),
        because dispatched messages may trigger UI updates, including layouts,
        which may lead panels to get destroyed and recreated, which can
        cause the screen to redraw outside of do_update() calls. so we dispatch
        messages, ensure our overlay is on top, then do a redraw. */
        Window::MessageQueue().Dispatch();

        if (this->state.overlayWindow) {
            this->state.overlay->BringToTop(); /* active overlay is always on top... */
        }

        /* always last to avoid flicker. see above. */
        Window::WriteToScreen(this->state.input);
    }

    overlays.Clear();
}

void App::UpdateFocusedWindow(IWindowPtr window) {
    if (this->state.focused != window) {
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
            newTopLayout->FocusFirst();
        }
    }
}

ILayoutPtr App::GetLayout() {
    return this->state.layout;
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
        this->state.rootWindow = dynamic_cast<IWindow*>(this->state.layout.get());

        if (this->state.rootWindow) {
            this->state.rootWindow->MoveAndResize(
                0, 0, Screen::GetWidth(), Screen::GetHeight());
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
