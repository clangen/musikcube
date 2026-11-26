#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "config.h"

#include <musikcore/plugin/playback-remote.hpp>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
    constexpr UINT HotkeyMessage = WM_APP + 42;
    HMODULE module = nullptr;
    class Hotkeys;
    std::atomic<Hotkeys*> installed { nullptr };

    class Hotkeys {
    public:
        explicit Hotkeys(const mc_plugin_services& services)
        : host(services.playback), stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
            if (!stopEvent) throw std::runtime_error("cannot create hotkey stop event");
        }
        ~Hotkeys() noexcept {
            RequestStop();
            Join();
            CloseHandle(stopEvent);
        }

        mc_result Start() {
            if (!host) return MC_ERROR_UNSUPPORTED;
            /* Avoid installing a global hook in console and debug sessions. */
            if (GetConsoleWindow() || IsDebuggerPresent()) return MC_OK;

            host->read_state(host->context, [](void* data, const mc_playback_snapshot* state, const mc_track_ref*) {
                static_cast<Hotkeys*>(data)->volume.store(state->volume);
            }, this);
            accepting.store(true);
            worker = std::thread([this] { Run(); });
            std::unique_lock lock(startupMutex);
            startupChanged.wait(lock, [this] { return ready; });
            return installedSuccessfully ? MC_OK : MC_ERROR_IO;
        }

        void Changed(const mc_plugin_playback_event& event) noexcept {
            volume.store(event.playback.volume);
        }
        void QueueChanged() noexcept {}
        void RequestStop() noexcept {
            accepting.store(false);
            SetEvent(stopEvent);
        }
        mc_result Join() {
            if (worker.joinable()) worker.join();
            return MC_OK;
        }

        bool Send(mc_playback_command command, double value = 0) noexcept {
            if (!accepting.load()) return false;
            const auto result = host->control_async(host->context, command, value,
                [](void*, mc_result) {}, nullptr);
            return result == MC_OK;
        }

        bool AdjustVolume(bool up) noexcept {
            const double current = volume.load();
            const double percent = std::round(current * 100.0);
            const double step = up ? (percent >= 10.0 ? 0.05 : 0.01)
                                   : (percent > 10.0 ? 0.05 : 0.01);
            const double next = std::clamp(current + (up ? step : -step), 0.0, 1.0);
            if (!Send(MC_PLAYBACK_SET_VOLUME, next)) return false;
            volume.store(next);
            return true;
        }

    private:
        static LRESULT CALLBACK Hook(int code, WPARAM message, LPARAM data) {
            auto* self = installed.load();
            if (code == HC_ACTION && self && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) {
                const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
                switch (key->vkCode) {
                    case VK_MEDIA_NEXT_TRACK:
                    case VK_MEDIA_PLAY_PAUSE:
                    case VK_MEDIA_PREV_TRACK:
                    case VK_MEDIA_STOP:
                        PostThreadMessageW(GetCurrentThreadId(), HotkeyMessage, key->vkCode, 0);
                        break;
                    default: {
                        const bool control = (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
                        const bool alt = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
                        if (!control || !alt) break;
                        switch (key->vkCode) {
                            case VK_F1: case VK_F2: case VK_F3: case VK_F4:
                            case VK_F5: case VK_F6: case VK_F7: case VK_F8:
                            case 'J': case 'L': case 'K': case 'I': case 'M':
                                if (self->accepting.load() &&
                                    PostThreadMessageW(GetCurrentThreadId(), HotkeyMessage, key->vkCode, 0) &&
                                    key->vkCode != VK_F8) return 1;
                                break;
                        }
                    }
                }
            }
            return CallNextHookEx(nullptr, code, message, data);
        }

        void DispatchHotkey(WPARAM key) noexcept {
            switch (key) {
                case VK_MEDIA_PLAY_PAUSE: case VK_F1: Send(MC_PLAYBACK_TOGGLE_PAUSE); break;
                case VK_MEDIA_STOP: case VK_F2: Send(MC_PLAYBACK_STOP); break;
                case VK_MEDIA_PREV_TRACK: case VK_F3: case 'J': Send(MC_PLAYBACK_PREVIOUS); break;
                case VK_MEDIA_NEXT_TRACK: case VK_F4: case 'L': Send(MC_PLAYBACK_NEXT); break;
                case VK_F5: case 'K': AdjustVolume(false); break;
                case VK_F6: case 'I': AdjustVolume(true); break;
                case VK_F7: case 'M': Send(MC_PLAYBACK_TOGGLE_MUTE); break;
                case VK_F8: Send(MC_PLAYBACK_TOGGLE_SHUFFLE); break;
            }
        }

        void Run() noexcept {
            /* Low-level hooks receive callbacks on the installing thread. */
            MSG message {};
            PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
            Hotkeys* empty = nullptr;
            HHOOK hook = nullptr;
            if (installed.compare_exchange_strong(empty, this)) {
                hook = SetWindowsHookExW(WH_KEYBOARD_LL, Hook, module, 0);
                if (!hook) installed.store(nullptr);
            }
            {
                std::lock_guard lock(startupMutex);
                installedSuccessfully = hook != nullptr;
                ready = true;
            }
            startupChanged.notify_all();
            if (!hook) return;

            for (;;) {
                const auto result = MsgWaitForMultipleObjects(1, &stopEvent, FALSE, INFINITE, QS_ALLINPUT);
                if (result != WAIT_OBJECT_0 + 1) break;
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    /* Return from the low-level hook before calling the host: the
                       foreground app may be waiting for that hook to finish. */
                    if (message.message == HotkeyMessage) {
                        DispatchHotkey(message.wParam);
                        continue;
                    }
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            UnhookWindowsHookEx(hook);
            installed.store(nullptr);
        }

        const mc_plugin_playback_service* host;
        HANDLE stopEvent;
        std::atomic<bool> accepting { false };
        std::atomic<double> volume { 1.0 };
        std::mutex startupMutex;
        std::condition_variable startupChanged;
        bool ready = false;
        bool installedSuccessfully = false;
        std::thread worker;
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterPlaybackRemote<Hotkeys>(*host);
    }
    void MC_CALL Shutdown(void*) {}
}

BOOL APIENTRY DllMain(HMODULE handle, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) module = handle;
    return TRUE;
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* api, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, api, error);
    if (result != MC_OK) return result;
    *api = MC_PLUGIN_API_DEFAULT;
    api->id = { "e2678930-ecd4-43b8-85e0-e41d634445b2", 36 };
    api->name = { "win32globalhotkeys", 18 };
    api->version = { "4.0.0", 5 };
    api->author = { "clangen", 7 };
    api->initialize = Initialize;
    api->shutdown = Shutdown;
    return MC_OK;
}
