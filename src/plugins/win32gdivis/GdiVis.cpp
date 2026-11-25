#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <musikcore/plugin/visualizer.hpp>

#include "MemoryDC.h"
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

namespace {
    constexpr wchar_t WindowClass[] = L"GdiVis-musikcube";
    constexpr size_t SpectrumBins = 256;
    HINSTANCE module = nullptr;

    /* The UI thread owns the window and timer. Playback only tries to copy
    into a fixed snapshot; it never waits for painting or allocates. */
    class GdiVisualizer {
    public:
        static constexpr mc_visualizer_inputs Inputs = MC_VISUALIZER_INPUT_SPECTRUM;
        static const char* Name() { return "GdiVis"; }
        explicit GdiVisualizer(const mc_plugin_services&) {}
        ~GdiVisualizer() noexcept { Hide(); }

        mc_result Show() {
            std::lock_guard lock(controlMutex);
            if (visible.load()) {
                return MC_OK;
            }
            if (worker.joinable()) {
                worker.join();
            }

            {
                std::lock_guard startupLock(startupMutex);
                ready = false;
                started = false;
            }
            worker = std::thread([this] { Run(); });
            std::unique_lock startupLock(startupMutex);
            startupChanged.wait(startupLock, [this] { return ready; });
            return started ? MC_OK : MC_ERROR_IO;
        }

        mc_result Hide() noexcept {
            std::lock_guard lock(controlMutex);
            if (worker.joinable()) {
                if (visible.load()) {
                    PostThreadMessageW(threadId, WM_QUIT, 0, 0);
                }
                worker.join();
            }
            return MC_OK;
        }

        mc_result IsVisible(bool& result) {
            result = visible.load();
            return MC_OK;
        }

        mc_result WriteSpectrum(const float* bins, size_t count, uint32_t) noexcept {
            if (!visible.load() || !bins) {
                return MC_OK;
            }
            std::unique_lock lock(spectrumMutex, std::try_to_lock);
            if (lock) {
                const auto copied = std::min(count, spectrum.size());
                std::copy_n(bins, copied, spectrum.begin());
                std::fill(spectrum.begin() + copied, spectrum.end(), 0.0f);
            }
            return MC_OK;
        }

    private:
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
            if (message == WM_NCCREATE) {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            }
            auto* self = reinterpret_cast<GdiVisualizer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (message) {
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
                case WM_CHAR:
                    if (wparam == VK_ESCAPE) {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    break;
                case WM_TIMER:
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case WM_PAINT:
                    if (self) {
                        self->Paint(hwnd);
                        return 0;
                    }
                    break;
            }
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }

        void Paint(HWND hwnd) {
            PAINTSTRUCT paint {};
            HDC dc = BeginPaint(hwnd, &paint);
            std::array<float, SpectrumBins> frame {};
            {
                std::lock_guard lock(spectrumMutex);
                frame = spectrum;
            }
            {
                win32cpp::MemoryDC memory(dc, paint.rcPaint);
                HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));
                RECT client {};
                GetClientRect(hwnd, &client);
                FillRect(memory, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                int x = 20;
                const int bottom = client.bottom - 15;
                for (size_t i = 0; i < SpectrumBins / 4; ++i) {
                    const int height = 4 + static_cast<int>(std::clamp(frame[i], 0.0f, 80.0f));
                    for (int y = bottom; y > bottom - height; y -= 2) {
                        RECT bar { x, y - 1, x + 7, y };
                        FillRect(memory, &bar, brush);
                    }
                    x += 8;
                }
                if (brush) DeleteObject(brush);
            }
            EndPaint(hwnd, &paint);
        }

        void Run() noexcept {
            MSG message {};
            PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
            threadId = GetCurrentThreadId();

            WNDCLASSW windowClass {};
            windowClass.hInstance = module;
            windowClass.lpfnWndProc = WindowProc;
            windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            windowClass.lpszClassName = WindowClass;
            const auto registered = RegisterClassW(&windowClass);
            const bool available = registered || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
            HWND window = available ? CreateWindowW(WindowClass, L"GdiVis-musikcube", WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 550, 140, nullptr, nullptr, module, this) : nullptr;
            if (window) {
                SetTimer(window, 1, 100, nullptr);
                ShowWindow(window, SW_SHOW);
                visible.store(true);
            }
            {
                std::lock_guard lock(startupMutex);
                started = window != nullptr;
                ready = true;
            }
            startupChanged.notify_all();

            if (window) {
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                if (IsWindow(window)) DestroyWindow(window);
            }
            visible.store(false);
            if (registered) UnregisterClassW(WindowClass, module);
        }

        std::mutex controlMutex;
        std::mutex startupMutex;
        std::condition_variable startupChanged;
        bool ready = false;
        bool started = false;
        std::atomic<bool> visible { false };
        DWORD threadId = 0;
        std::thread worker;
        std::mutex spectrumMutex;
        std::array<float, SpectrumBins> spectrum {};
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterVisualizer<GdiVisualizer>(*host);
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
    api->id = { "4b0f37f7-96fe-438c-acf2-bbdd53ce1ddf", 36 };
    api->name = { "GdiVis", 6 };
    api->version = { "4.0.0", 5 };
    api->author = { "clangen", 7 };
    api->initialize = Initialize;
    api->shutdown = Shutdown;
    return MC_OK;
}
