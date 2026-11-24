#include "CddaMediaMonitor.h"

#include <dbt.h>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <stdexcept>

CddaMediaMonitor::CddaMediaMonitor(const mc_indexer_source_host_api& service)
: service(service) {
    /* Start only after every member used by Run has been constructed. */
    this->worker = std::thread([this] { this->Run(); });
    std::unique_lock lock(this->mutex);
    this->ready.wait(lock, [this] {
        return this->started;
    });
    if (!this->available) {
        lock.unlock();
        this->worker.join();
        throw std::runtime_error("cannot monitor CD drive changes");
    }
}

CddaMediaMonitor::~CddaMediaMonitor() {
    if (this->worker.joinable()) {
        if (!PostThreadMessageW(this->threadId, WM_QUIT, 0, 0)) {
            std::fputs("CDDA monitor: cannot stop device notification thread\n", stderr);
            std::abort();
        }
        this->worker.join();
    }
}

bool CddaMediaMonitor::MediaRemoved(char letter) const noexcept {
    return this->volumeChanges.Removed(letter);
}

uint64_t CddaMediaMonitor::ChangeSnapshot() const noexcept {
    return this->volumeChanges.Snapshot();
}

void CddaMediaMonitor::ConfirmMediaPresent(char letter, uint64_t snapshot) noexcept {
    this->volumeChanges.ConfirmPresent(letter, snapshot);
}

LRESULT CALLBACK CddaMediaMonitor::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* monitor = reinterpret_cast<CddaMediaMonitor*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (monitor && message == WM_DEVICECHANGE && (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE) &&
        lparam)
    {
        const auto* info = reinterpret_cast<const DEV_BROADCAST_HDR*>(lparam);
        if (info->dbch_devicetype == DBT_DEVTYP_VOLUME) {
            if (info->dbch_size >= sizeof(DEV_BROADCAST_VOLUME)) {
                const auto* volume = reinterpret_cast<const DEV_BROADCAST_VOLUME*>(info);
                monitor->volumeChanges.Notify(volume->dbcv_unitmask, wparam == DBT_DEVICEREMOVECOMPLETE);
            }
            monitor->service.request_rescan(monitor->service.context);
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void CddaMediaMonitor::Run() noexcept {
    /* The queue must exist before the destructor can post WM_QUIT. */
    MSG message {};
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    const DWORD id = GetCurrentThreadId();
    wchar_t name[64] {};
    std::swprintf(name, std::size(name), L"musikcube-cdda-monitor-%lu", id);
    HINSTANCE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&CddaMediaMonitor::WindowProc), &module);

    WNDCLASSW windowClass {};
    windowClass.hInstance = module;
    windowClass.lpszClassName = name;
    windowClass.lpfnWndProc = &CddaMediaMonitor::WindowProc;
    const ATOM registered = RegisterClassW(&windowClass);
    HWND window =
        registered ? CreateWindowExW(0, name, L"", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, module, this) : nullptr;

    {
        std::lock_guard lock(this->mutex);
        this->threadId = id;
        this->available = window != nullptr;
        this->started = true;
        this->ready.notify_all();
    }

    if (window) {
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        DestroyWindow(window);
    }
    if (registered) {
        UnregisterClassW(name, module);
    }
}
