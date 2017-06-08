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

#include "MainWindow.h"

#include <core/config.h>
#include <core/runtime/MessageQueue.h>

#include <chrono>

static const DWORD WM_SCHEDULE_CORE_DISPATCH = WM_USER + 1;
static const UINT_PTR DISPATCH_TIMER_ID = 1;

using namespace musik::core::runtime;
using namespace musik::win;
using namespace std::chrono;

class MainWindow::Win32MessageQueue : public MessageQueue {
    public:
        Win32MessageQueue(HWND hwnd) : hwnd(hwnd) {
            MessageQueue();
            this->nextTimerTime = -1;
        }

        virtual void Post(IMessagePtr message, int64_t delayMs = 0) {
            MessageQueue::Post(message, delayMs);
            ::PostMessage(hwnd, WM_SCHEDULE_CORE_DISPATCH, 0, 0);
        }

        void Reset() {
            ::KillTimer(hwnd, DISPATCH_TIMER_ID);
            this->nextTimerTime = -1;
        }

        void ScheduleNext() {
            int64_t now = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count();

            int64_t next = this->GetNextMessageTime();

            bool dispatch =
                (nextTimerTime > 0 && next < this->nextTimerTime) ||
                (next > 0 && nextTimerTime <= 0);

            if (dispatch) {
                int64_t delayMs = next - now;
                if (delayMs > 0) {
                    ::SetTimer(hwnd, DISPATCH_TIMER_ID, (UINT)delayMs, nullptr);
                    this->nextTimerTime = next;
                }
                else {
                    ::PostMessage(this->hwnd, WM_TIMER, DISPATCH_TIMER_ID, 0);
                    ::KillTimer(this->hwnd, DISPATCH_TIMER_ID);
                    this->nextTimerTime = -1;
                }
            }
        }

    private:
        int64_t nextTimerTime;
        HWND hwnd;
};

MainWindow::MainWindow(const win32cpp::uichar* windowTitle)
: TopLevelWindow(windowTitle) {
    this->queue = nullptr;
}

MainWindow::~MainWindow() {
    delete this->queue;
}

LRESULT MainWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TIMER) {
        if (wParam == DISPATCH_TIMER_ID) {
            if (!this->queue) {
                this->queue = new Win32MessageQueue(this->Handle());
            }

            this->queue->Dispatch();
            this->queue->Reset();
            this->queue->ScheduleNext();
            return 0L;
        }
    }
    else if (message == WM_SCHEDULE_CORE_DISPATCH) {
        if (!this->queue) {
            this->queue = new Win32MessageQueue(this->Handle());
        }

        this->queue->ScheduleNext();
        return 0L;
    }

    return TopLevelWindow::WindowProc(message, wParam, lParam);
}

musik::core::runtime::IMessageQueue& MainWindow::Queue() {
    if (!this->queue && this->Handle()) {
        this->queue = new Win32MessageQueue(this->Handle());
    }

    return *(this->queue);
}
