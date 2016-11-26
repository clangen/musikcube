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
    if (code == HC_ACTION && playback) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
            switch (p->vkCode) {
                case VK_MEDIA_NEXT_TRACK:
                    playback->Next();
                    return 0;
                case VK_MEDIA_PLAY_PAUSE:
                    playback->PauseOrResume();
                    return 0;
                case VK_MEDIA_PREV_TRACK:
                    playback->Previous();
                    return 0;
                case VK_MEDIA_STOP:
                    playback->Stop();
                    return 0;
            }

            bool ctrl = GetAsyncKeyState(VK_RCONTROL) & 0x8000;
            bool alt = GetAsyncKeyState(VK_RMENU) & 0x8000;
            //bool win = GetAsyncKeyState(VK_LWIN) & 0x8000;

            if (ctrl && alt) {
                switch (p->vkCode) {
                    case VK_F1:
                        playback->PauseOrResume();
                        return 1;

                    case VK_F2:
                        playback->Stop();
                        return 1;

                    case VK_F3:
                    case 'J':
                    case 'j':
                        playback->Previous();
                        return 1;

                    case VK_F4:
                    case 'L':
                    case 'l':
                        playback->Next();
                        return 1;

                    case VK_F5:
                    case 'K':
                    case 'k':
                        playback->SetVolume(playback->GetVolume() - 0.05);
                        return 1;

                    case VK_F6:
                    case 'I':
                    case 'i':
                        playback->SetVolume(playback->GetVolume() + 0.05);
                        return 1;

                    case VK_F7:
                    case 'M':
                    case 'm':
                        playback->ToggleMute();
                        return 1;

                    case VK_F8:
                        playback->ToggleShuffle();
                        return 0;
                }
            }
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) ShellProc, module, 0L);
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

            virtual void OnPlaybackStateChanged(musik::core::sdk::PlaybackState state) {

            }

            virtual void OnVolumeChanged(double volume) {

            }

            virtual void OnModeChanged(musik::core::sdk::RepeatMode repeatMode, bool shuffled) {

            }
};

static MMShellHook plugin;

extern "C" __declspec(dllexport) musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" __declspec(dllexport) musik::core::sdk::IPlaybackRemote* GetPlaybackRemote() {
    return &plugin;
}