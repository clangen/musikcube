#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <algorithm>

#include <shlwapi.h>

#include <core/sdk/ISpectrumVisualizer.h>
#include <core/sdk/IPlugin.h>

#include "MemoryDC.h"

#define DLL_EXPORT __declspec(dllexport)
#define MAX_FPS 10LL
#define MILLIS_PER_FRAME (1000LL / MAX_FPS)
#define WINDOW_CLASS_NAME L"GdiVis-musikcube"
#define DEFAULT_WIDTH 550
#define DEFAULT_HEIGHT 120
#define TEXTURE_WIDTH 90
#define TEXTURE_HEIGHT 80

using namespace std::chrono;

static std::atomic<bool> quit(false);
static std::atomic<bool> thread(false);
static std::mutex pcmMutex, threadMutex;
static std::condition_variable threadCondition;
static HINSTANCE instance;
static HBITMAP texture = nullptr;
static COLORREF *textureBits = nullptr;
static HBRUSH fg = CreateSolidBrush(RGB(255, 0, 0));
static COLORREF bg = RGB(24, 24, 24);

static int spectrumSize = 0;
static float* spectrumIn = nullptr;
static float* spectrumOut = nullptr;

static void renderFrame(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT& r = ps.rcPaint;

    {
        /* copy to the output buffer so we can write the input buffer
        immediately if we want to. */
        std::unique_lock<std::mutex> lock(pcmMutex);
        memcpy(::spectrumOut, ::spectrumIn, ::spectrumSize * sizeof(float));
    }

    if (::spectrumOut && ::spectrumSize) {

        win32cpp::MemoryDC memDc(hdc, ps.rcPaint);

        for (int n = 0; n < TEXTURE_WIDTH * TEXTURE_HEIGHT - 1; n++) {
            textureBits[n] = bg;
        }

        RECT rect;

        int barHeight;
        const int barWidth = 7;
        const int barY = 100;

        int x = 20;
        int n = spectrumSize / 4;
        for (int bar = 0; bar < n; bar++) {
            barHeight = 4 + (int)(spectrumOut[bar]);
            rect.left = x;
            rect.right = rect.left + barWidth;

            int dstY = barY - barHeight;
            for (int y = barY; y > dstY; y-=2) {
                rect.top = y;
                rect.bottom = rect.top - 1;
                FillRect(memDc, &rect, fg);
            }

            x += barWidth + 1;
        }
    }

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK staticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            UnregisterClass(WINDOW_CLASS_NAME, NULL);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            break;

        case WM_CHAR:
            if (wParam == 0x1B) { /* esc */
                PostQuitMessage(0);
            }
            break;

        case WM_PAINT:
            renderFrame(hwnd);
            break;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static void threadProc() {
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetBreakAlloc(60);
#endif

    // Register the windows class
    WNDCLASSW wndClass;
    wndClass.style = CS_DBLCLKS;
    wndClass.lpfnWndProc = staticWndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = instance;
    wndClass.hIcon = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass(&wndClass)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
    }

    int windowWidth = DEFAULT_WIDTH;
    int windowHeight = DEFAULT_HEIGHT;

    RECT rc;
    SetRect(&rc, 0, 0, windowWidth, windowHeight);
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

    // Create the render window
    HWND hwnd = CreateWindow(
        WINDOW_CLASS_NAME,
        L"GdiVis-musikcube",
        WS_EX_TOOLWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        (rc.right - rc.left),
        (rc.bottom - rc.top),
        0,
        NULL,
        instance,
        0);

    if (!hwnd) {
        DWORD dwError = GetLastError();
        return;
    }

    BITMAPINFO bminfo;
    ZeroMemory(&bminfo, sizeof(BITMAPINFO));
    bminfo.bmiHeader.biWidth = TEXTURE_WIDTH;
    bminfo.bmiHeader.biHeight = TEXTURE_HEIGHT;
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;
    bminfo.bmiHeader.biXPelsPerMeter = 0;
    bminfo.bmiHeader.biYPelsPerMeter = 0;
    bminfo.bmiHeader.biClrUsed = 0;
    bminfo.bmiHeader.biClrImportant = 0;
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biSizeImage = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4;

    texture = CreateDIBSection(NULL, &bminfo, DIB_RGB_COLORS, (void **)&textureBits, NULL, NULL);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    msg.message = WM_NULL;

    PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            if (quit) {
                PostQuitMessage(0);
            }
            else {
                DWORD start = GetTickCount();
                InvalidateRect(hwnd, 0, false);
                UpdateWindow(hwnd);
                DWORD delta = (GetTickCount() - start);
                if (MILLIS_PER_FRAME > delta) {
                    Sleep(MILLIS_PER_FRAME - delta);
                }
            }
        }
    }

    DeleteObject(texture);
    texture = nullptr;
    textureBits = nullptr;

    thread.store(false);

    {
        std::unique_lock<std::mutex> lock(threadMutex);
        threadCondition.notify_all();
    }
}

#ifdef COMPILE_AS_EXE
    int main(int argc, char *argv[]) {
        SetProjectMDataDirectory(util::getModuleDirectory());

#ifndef WIN32
        std::thread background(pipeReadProc);
        background.detach();
#endif

        quit.store(false);
        thread.store(true);
        windowProc();

        return 0;
    }
#else
    #ifdef WIN32
        BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
            instance = hModule;
            return true;
        }
    #endif
#endif

#ifdef WIN32
    class Visualizer : public musik::core::sdk::ISpectrumVisualizer {
        public:
            virtual const char* Name() {
                return "GdiVis";
            };

            virtual const char* Version() {
                return "0.1.0";
            };

            virtual const char* Author() {
                return "clangen";
            };

            virtual void Destroy() {
                this->Hide();
                delete this;
            }

            virtual void Write(float *spectrum, int size) {
                std::unique_lock<std::mutex> lock(pcmMutex);

                if (::spectrumSize != size) {
                    delete ::spectrumIn;
                    delete ::spectrumOut;
                    ::spectrumSize = size;
                    ::spectrumIn = new float[size];
                    ::spectrumOut = new float[size];
                }

                memcpy(::spectrumIn, spectrum, size * sizeof(float));
            }

            virtual void Show() {
                if (!Visible()) {
                    quit.store(false);
                    thread.store(true);
                    std::thread background(threadProc);
                    background.detach();
                }
            }

            virtual void Hide() {
                if (Visible()) {
                    quit.store(true);

                    while (thread.load()) {
                        std::unique_lock<std::mutex> lock(threadMutex);
                        threadCondition.wait(lock);
                    }
                }
            }

            virtual bool Visible() {
                return thread.load();
            }
    };

    extern "C" DLL_EXPORT musik::core::sdk::IPlugin* GetPlugin() {
        return new Visualizer();
    }

    extern "C" DLL_EXPORT musik::core::sdk::ISpectrumVisualizer* GetSpectrumVisualizer() {
        return new Visualizer();
    }
#endif
