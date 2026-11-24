#pragma once

#include <musikcore/plugin/indexer.h>
#include "CddaMediaStatus.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <windows.h>

/* A hidden top-level window receives Windows volume arrival/removal messages.
The host owns the rescan service until this monitor has stopped and joined. */
class CddaMediaMonitor {
public:
    explicit CddaMediaMonitor(const mc_indexer_source_host_api& service);
    ~CddaMediaMonitor();
    CddaMediaMonitor(const CddaMediaMonitor&) = delete;
    CddaMediaMonitor& operator=(const CddaMediaMonitor&) = delete;
    bool MediaRemoved(char letter) const noexcept;
    uint64_t ChangeSnapshot() const noexcept;
    void ConfirmMediaPresent(char letter, uint64_t snapshot) noexcept;

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void Run() noexcept;

    const mc_indexer_source_host_api& service;
    CddaVolumeChanges volumeChanges;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable ready;
    DWORD threadId = 0;
    bool started = false;
    bool available = false;
};
