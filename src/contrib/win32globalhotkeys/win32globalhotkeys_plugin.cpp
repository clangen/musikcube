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

#include "pch.h"

#include <core/sdk/IPlugin.h>
#include <core/sdk/IPlaybackRemote.h>

musik::core::sdk::IPlaybackService* playback;

static HHOOK hook = NULL;

LRESULT CALLBACK ShellProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HSHELL_APPCOMMAND && playback) {
        switch (GET_APPCOMMAND_LPARAM(lParam)) {
        case APPCOMMAND_MEDIA_NEXTTRACK:
            playback->Next();
            return 0;
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            playback->PauseOrResume();
            return 0;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK:
            playback->Previous();
            return 0;
        case APPCOMMAND_MEDIA_STOP:
            playback->Stop();
            return 0;
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            hook = SetWindowsHookEx(WH_SHELL, (HOOKPROC) ShellProc, module, 0L);
            break;

        case DLL_PROCESS_DETACH:
            UnhookWindowsHookEx(hook);
            hook = nullptr;
            break;
    }

    return TRUE;
}

class MMShellHook:
    public musik::core::sdk::IPlugin,
    public musik::core::sdk::IPlaybackRemote {
        public:
            void Destroy() {
            }

            const char* Name() {
                return "win32globalhotkeys";
            }

            const char* Version() {
                return "0.1";
            }

            const char* Author() {
                return "clangen";
            }

            virtual void SetPlaybackService(musik::core::sdk::IPlaybackService* playback) {
                ::playback = playback;
            }

            virtual void OnTrackChanged(musik::core::sdk::ITrack* track) {
            }
};

static MMShellHook plugin;

extern "C" __declspec(dllexport) musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" __declspec(dllexport) musik::core::sdk::IPlaybackRemote* GetPlaybackRemote() {
    return &plugin;
}