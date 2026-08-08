//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/** @file App.h @brief The top-level application driver for the cursespp toolkit. */
#pragma once

#include <queue>
#include <functional>
#include <cursespp/ILayout.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/OverlayStack.h>
#include <cursespp/Colors.h>

namespace cursespp {
    /** @brief The main application object that drives the curses-based UI.
     *
     *  @details App owns the ncurses/PDCurses terminal, the event loop, the
     *  active layout and the modal overlay stack. It is a singleton accessible
     *  via App::Instance(). A root ILayout is installed with Run() and becomes
     *  the root of the window tree. Keyboard input is read from the terminal,
     *  normalized to strings (see IKeyHandler) and dispatched to the active
     *  layout -- or to the top-most overlay, which consumes all input while
     *  visible. Custom color palettes, mouse handling, resize hooks and the
     *  quit key can be configured before the event loop starts.
     *
     *  @note App is not designed to be subclassed; its destructor is non-virtual.
     */
    class App {
        public:
            /** @brief Callback invoked for each key press; return true to consume the key. */
            using KeyHandler = std::function<bool(const std::string&)>;
            /** @brief Callback invoked whenever the terminal dimensions change. */
            using ResizeHandler = std::function<void()>;

            /** @brief Initializes the application and the curses layer.
             *  @param title the initial window title.
             */
            App(const std::string& title);
            /** @brief Shuts down curses and restores the terminal to its prior state. */
            ~App(); /* do not subclass */

            /** @brief Non-copyable. */
            App(const App& other) = delete;
            /** @brief Non-assignable. */
            App& operator=(const App& other) = delete;

            /** @brief Replaces the global key handler used to consume application-level keys.
             *  @param handler the new key handler, or nullptr to clear it.
             */
            void SetKeyHandler(KeyHandler handler);
            /** @brief Installs a key hook that observes every key before it is dispatched.
             *  @param hook the hook callback, or nullptr to remove it.
             */
            void SetKeyHook(KeyHandler hook);
            /** @brief Registers a callback invoked when the terminal is resized.
             *  @param handler the resize callback.
             */
            void SetResizeHandler(ResizeHandler handler);
            /** @brief Selects the color mode (palette, 256-color or RGB) used by the UI.
             *  @param mode the desired Colors::Mode.
             */
            void SetColorMode(Colors::Mode mode);
            /** @brief Selects how the background color is resolved (theme or terminal).
             *  @param bgType the desired Colors::BgType.
             */
            void SetColorBackgroundType(Colors::BgType bgType);
            /** @brief Loads a named color theme from the theme directory.
             *  @param name the theme name.
             */
            void SetColorTheme(const std::string& name);
            /** @brief Sets the smallest terminal size the application will tolerate.
             *  @param width the minimum width in cells.
             *  @param height the minimum height in cells.
             */
            void SetMinimumSize(int width, int height);
            /** @brief Enables or disables mouse event reporting.
             *  @param enabled true to process mouse input.
             */
            void SetMouseEnabled(bool enabled);
            /** @brief Returns true if a modal overlay is currently visible. */
            bool IsOverlayVisible() { return this->state.overlay != nullptr; }
            /** @brief Controls whether the window can be minimized to the system tray.
             *  @param minimizeToTray true to allow tray minimization.
             */
            void SetMinimizeToTray(bool minimizeToTray);
            /** @brief Sets the window title shown in the terminal/title bar.
             *  @param title the new title.
             */
            void SetTitle(const std::string& title);
            /** @brief Returns the key string bound to the quit action.
             *  @return the normalized quit key, e.g. "^D".
             */
            std::string GetQuitKey();
            /** @brief Binds a new key string to the quit action.
             *  @param kn the normalized key, e.g. "^D" or "F10".
             */
            void SetQuitKey(const std::string& kn);
            /** @brief Minimizes the application window (platform dependent). */
            void Minimize();
            /** @brief Restores a previously minimized window. */
            void Restore();
            /** @brief Informs the framework that the terminal was resized and triggers a relayout. */
            void NotifyResized();
            /** @brief Re-arranges all windows to fit the current terminal dimensions. */
            void Layout();

#ifdef WIN32
            /** @brief On Windows, checks whether another instance of the app is already running.
             *  @param uniqueId the single-instance identifier.
             *  @param title the window title to match.
             *  @return true if a previous instance is active.
             */
            static bool Running(const std::string& uniqueId, const std::string& title);
            /** @brief Single-instance check using the default identifier.
             *  @param title the window title to match.
             *  @return true if a previous instance is active.
             */
            static bool Running(const std::string& title);
            /** @brief Sets the icon resource used for the window and taskbar.
             *  @param resourceId the Win32 resource id of the icon.
             */
            void SetIcon(int resourceId);
            /** @brief Sets the single-instance identifier used by the app.
             *  @param uniqueId the unique id string.
             */
            void SetSingleInstanceId(const std::string& uniqueId);
            /** @brief Registers a custom console font with Windows.
             *  @param filename path to the font file.
             *  @return true if the font was registered.
             */
            static bool RegisterFont(const std::string& filename);
            /** @brief Sets the default console font face used on Windows.
             *  @param fontface the font family name.
             */
            static void SetDefaultFontface(const std::string& fontface);
            /** @brief Controls the visibility of the window's system menu.
             *  @param visible true to show the menu.
             */
            static void SetDefaultMenuVisibility(bool visible);
#endif

            /** @brief Installs the root layout and runs the application event loop.
             *  @param layout the root ILayout to display.
             */
            void Run(ILayoutPtr layout);
            /** @brief Replaces the root layout with a new one.
             *  @param layout the new root layout.
             */
            void ChangeLayout(ILayoutPtr layout);
            /** @brief Returns the current root layout.
             *  @return the active ILayoutPtr.
             */
            ILayoutPtr GetLayout();
            /** @brief Injects a synthetic key press into the input pipeline.
             *  @param key the normalized key string to dispatch.
             */
            void InjectKeyPress(const std::string& key);
            /** @brief Terminates the application event loop. */
            void Quit();

            /** @brief Returns the process-wide singleton application instance.
             *  @return a reference to the App singleton.
             */
            static App& Instance();

            /** @brief Returns a monotonic millisecond timestamp for scheduling.
             *  @return the current time in milliseconds.
             */
            static int64_t Now();
            /** @brief Returns the application's overlay stack.
             *  @return a reference to the shared OverlayStack.
             */
            static OverlayStack& Overlays();

        private:
            /** @brief Aggregates the runtime state of the application. */
            struct WindowState {
                ILayoutPtr overlay;                       /**< The currently visible modal overlay, if any. */
                IWindow* overlayWindow{ nullptr };        /**< The window instance hosting the overlay. */
                ILayoutPtr layout;                        /**< The root layout. */
                IWindowPtr focused;                       /**< The currently focused window. */
                IWindow* rootWindow{ nullptr };           /**< The root curses window handle owner. */
                IInput* input{ nullptr };                 /**< The active text input widget, if any. */
                IKeyHandler* keyHandler{ nullptr };       /**< The active key handler, if any. */

                inline ILayoutPtr ActiveLayout() {
                    /* if there's a visible overlay, it's always the current
                    layout and will consume all key events */
                    return overlay ? overlay : layout;
                }
            };

            void InitCurses();
            void UpdateFocusedWindow(IWindowPtr window);
            void EnsureFocusIsValid();
            void CheckShowOverlay();
            void FocusNextInLayout();
            void FocusPrevInLayout();
            void OnResized();

            std::queue<std::string> injectedKeys;         /**< Keys queued by InjectKeyPress. */
            WindowState state;                            /**< The current runtime state. */
            KeyHandler keyHandler, keyHook;               /**< Application key handler and observation hook. */
            ResizeHandler resizeHandler;                  /**< Callback invoked on terminal resize. */
            Colors::Mode colorMode { Colors::Palette };   /**< Active color mode. */
            Colors::BgType bgType { Colors::Theme };      /**< Background color resolution strategy. */
            std::string colorTheme;                       /**< Name of the active color theme. */
            std::string quitKey{"^D"};                    /**< Key string that terminates the app. */
            int minWidth, minHeight;                      /**< Minimum accepted terminal dimensions. */
            bool mouseEnabled{true};                      /**< Whether mouse events are processed. */
            bool quit{false}, initialized{false};         /**< Lifecycle flags. */
            std::string appTitle;                         /**< The application window title. */

#ifdef WIN32
            int iconId;                                   /**< Icon resource id used on Windows. */
            std::string uniqueId;                         /**< Single-instance identifier on Windows. */
#endif
    };
}
