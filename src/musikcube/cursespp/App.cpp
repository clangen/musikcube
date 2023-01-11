//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
static bool resized = false;

static App* instance = nullptr;

#ifndef WIN32
static void hangupHandler(int signal) {
    disconnected = true;
}

static void resizedHandler(int signal) {
    endwin(); /* required in *nix because? */
    resized = true;
}

static std::string getEnvironmentVariable(const std::string& name) {
    std::string result;
    const char* value = std::getenv(name.c_str());
    if (value) {
        result = value;
    }
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static bool containsUtf8(const std::string& value) {
    return
        value.find("utf-8") != std::string::npos ||
        value.find("utf8") != std::string::npos;
}

static bool isLangUtf8() {
    /* https://github.com/clangen/musikcube/issues/324 */

    std::string lcAll = getEnvironmentVariable("LC_ALL");
    std::string lcCType = getEnvironmentVariable("LC_CTYPE");
    std::string lang = getEnvironmentVariable("LANG");

    if (lcAll.size()) {
        return containsUtf8(lcAll);
    }

    if (lcCType.size()) {
        return containsUtf8(lcCType);
    }

    return containsUtf8(lang);
}
#endif

#ifdef WIN32
static void pdcWinguiResizeCallback() {
    App::Instance().NotifyResized();
    App::Instance().Layout();
}
#endif

/* the way curses represents mouse button state is really gross, and makes it
annoying to process click events with low latency while also supporting double
clicks and multiple buttons. this structure is used to maintain and manipulate
state related to mouse buttons, and is used by the main loop of the app to
generate button click events for Window instances to handle. beware: kludge
and hacks below... */
struct MouseButtonState {
    bool Update(const MEVENT& rawEvent) {
        const mmask_t state = rawEvent.bstate;
        const int x = rawEvent.x;
        const int y = rawEvent.y;
        bool result = false;
        const int64_t newTime = App::Now();

        /* ugh... */
        const bool isDown = state & BUTTON1_PRESSED || state & BUTTON2_PRESSED || state & BUTTON3_PRESSED;
        const bool isUp = state & BUTTON1_RELEASED || state & BUTTON2_RELEASED || state & BUTTON3_RELEASED;

        if (!isDown && !isUp) {
            /* if both of these were false we got an unexpected mouse event. let's
            go ahead and reset our state and return */
            Reset();
            return false;
        }

        int newButton = 0;

        if (state & BUTTON1_PRESSED || state & BUTTON1_RELEASED) {
            newButton = 1;
        }
        if (state & BUTTON2_PRESSED || state & BUTTON2_RELEASED) {
            newButton = 2;
        }
        if (state & BUTTON3_PRESSED || state & BUTTON3_RELEASED) {
            newButton = 3;
        }

        /* threshold for double click, in milliseconds*/
        const bool elapsed = newTime - time > 300;

        if ((!elapsed && lastX >= 0 && lastY >= 0) && (std::abs(x - lastX) > 2 || std::abs(y - lastY) > 2)) {
            /* cursor traveled too far, reset state and convert back to a single click */
            this->Reset();
        }

        if (newButton != button || elapsed) {
            /* if the button changed or there was too much time between state
            changes, go ahead and reset now */
            Reset();
        }

        if (isDown) {
            /* to improve latency response we treat the leading edge of the first
            click as the event, and don't wait for a "release" event, because that
            can take hundreds of milliseconds. we kludge the `wasDown` flag to be
            false, so the subsequent click (below) can be "correctly" recorded
            as a double click. */
            if (!this->clicked) {
                this->clicked = true;
                this->wasDown = false;
            }
            else {
                this->wasDown = true;
            }
            result = true;
        }
        else if (clicked && wasDown && !isDown) {
            this->doubleClicked = true;
            this->wasDown = true;
            result = true;
        }

        this->button = newButton;
        this->time = newTime;
        this->lastX = x;
        this->lastY = y;
        return result;
    }

    void Reset() noexcept {
        button = 0;
        state = 0;
        time = 0;
        lastX = -1;
        lastY = -1;
        wasDown = false;
        clicked = false;
        doubleClicked = false;
    }

    mmask_t ToCursesState() noexcept {
        if (doubleClicked) {
            switch (button) {
            case 1: return BUTTON1_DOUBLE_CLICKED;
            case 2: return BUTTON2_DOUBLE_CLICKED;
            case 3: return BUTTON3_DOUBLE_CLICKED;
            default: break;
            }
        }
        else if (clicked) {
            switch (button) {
            case 1: return BUTTON1_CLICKED;
            case 2: return BUTTON2_CLICKED;
            case 3: return BUTTON3_CLICKED;
            default: break;
            }
        }
        return 0;
    }

    bool wasDown{ false };
    bool clicked{ false };
    bool doubleClicked{ false };
    int button;
    int state;
    int lastX{ -1 }, lastY{ -1 };
    int64_t time{ -1 };
};

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

#ifndef WIN32
    if (!isLangUtf8()) {
        std::cout << "\n\nThis application requires a UTF-8 compatible LANG environment "
        "variable to be set in the controlling terminal. Setting to C.UTF-8. Depending on "
        "your environment, you may see corrupted output. If that'st he case, try to set "
        "LANG=C.UTF-8 before starting musikcube.\n\n\n\n";
        setenv("LANG", "C.UTF-8", 1);
    }
#endif

    instance = this; /* only one instance. */
    this->quit = false;
    this->minWidth = this->minHeight = 0;
    this->mouseEnabled = true;

#ifdef WIN32
    this->iconId = 0;
    this->colorMode = Colors::RGB;
    win32::ConfigureDpiAwareness();
#else
    setlocale(LC_ALL, "");
    std::signal(SIGHUP, hangupHandler);
    std::signal(SIGWINCH, resizedHandler);
    std::signal(SIGPIPE, SIG_IGN);
    this->colorMode = Colors::Palette;
#endif

    this->InitCurses();
    this->SetTitle(title);
}

App::~App() {
    endwin();
}

void App::InitCurses() {
    initscr();
    nonl();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();
    curs_set(0);
    mousemask(ALL_MOUSE_EVENTS, nullptr);
    mouseinterval(0);
    timeout(IDLE_TIMEOUT_MS);

#ifndef WIN32
    set_escdelay(20);
#endif

#ifdef WIN32
    PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, 4);
    #if defined PDCURSES_WINGUI && !defined PDCURSES_WINCON
        /* needs to happen after initscr() */
        PDC_set_default_menu_visibility(0);
        PDC_set_window_resized_callback(&pdcWinguiResizeCallback);
        PDC_set_title(this->appTitle.c_str());
        win32::ConfigureThemeAwareness();
        win32::InterceptWndProc();
        win32::SetAppTitle(this->appTitle);
        if (this->iconId) {
            this->SetIcon(this->iconId);
        }
    #endif
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
#if defined(PDCURSES_WINGUI) && !defined(PDCURSES_WINCON)
    PDC_set_preferred_fontface(u8to16(fontface).c_str());
#endif
}

void App::SetDefaultMenuVisibility(bool visible) {
#if defined(PDCURSES_WINGUI) && !defined(PDCURSES_WINCON)
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
#endif

    int64_t ch;
    std::string kn;

    this->state.input = nullptr;
    this->state.keyHandler = nullptr;
    int lastWidth = Screen::GetWidth();
    int lastHeight = Screen::GetHeight();

    this->ChangeLayout(layout);

    MEVENT rawMouseEvent;
    MouseButtonState mouseButtonState;

    while (!this->quit && !disconnected) {
        kn = "";

        if (this->injectedKeys.size()) {
            kn = injectedKeys.front();
            injectedKeys.pop();
            ch = ERR;
            goto process;
        }

        if (this->state.input && this->state.focused->GetContent()) {
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
#ifdef WIN32
            else if (kn == "KEY_RESIZE") {
                resized = true;
            }
#endif
            else if (this->mouseEnabled && kn == "KEY_MOUSE") {
#ifdef WIN32
                if (nc_getmouse(&rawMouseEvent) == 0) {
#else
                if (getmouse(&rawMouseEvent) == 0) {
#endif
                    auto active = this->state.ActiveLayout();
                    if (active) {
                        using Event = IMouseHandler::Event;
                        auto window = dynamic_cast<IWindow*>(active.get());
                        Event event(rawMouseEvent, window);
                        if (event.MouseWheelDown() || event.MouseWheelUp()) {
                            if (state.focused) {
                                state.focused->ProcessMouseEvent(event);
                            }
                        }
                        else if (mouseButtonState.Update(rawMouseEvent)) {
                            event.state = mouseButtonState.ToCursesState();
                            active->ProcessMouseEvent(event);
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

        resized |=
            lastWidth != Screen::GetWidth() ||
            lastHeight != Screen::GetHeight();
        if (resized) {
            lastWidth = Screen::GetWidth();
            lastHeight = Screen::GetHeight();
            this->NotifyResized();
        }

        this->Layout();
    }

    overlays.Clear();
}

void App::NotifyResized() {
    resized = false;
    resize_term(0, 0);
    Window::InvalidateScreen();
    if (this->resizeHandler) {
        this->resizeHandler();
    }
    this->OnResized();
}

void App::Layout() {
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
