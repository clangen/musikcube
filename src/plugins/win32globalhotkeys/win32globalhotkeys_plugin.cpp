//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IPlaybackRemote.h>

#include <cmath>

musik::core::sdk::IPlaybackService* playback;

static HHOOK hook = nullptr;
static HMODULE module = nullptr;

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

            short ctrl = GetAsyncKeyState(VK_RCONTROL) & 0x8000;
            short alt = GetAsyncKeyState(VK_RMENU) & 0x8000;
            //bool win = GetAsyncKeyState(VK_LWIN) & 0x8000;

            double delta;

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
                        delta = round(playback->GetVolume() * 100.0) > 10.0 ? 0.05 : 0.01;
                        playback->SetVolume(playback->GetVolume() - delta);
                        return 1;

                    case VK_F6:
                    case 'I':
                    case 'i':
                        delta = round(playback->GetVolume() * 100.0) >= 10.0 ? 0.05 : 0.01;
                        playback->SetVolume(playback->GetVolume() + delta);
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

void installHook() {
    /* note: don't install the hook if we're debugging or running as a true console
    app, otherwise inputs get SUPER laggy. */
    bool isConsoleApp = GetConsoleWindow() != nullptr;
    if (!isConsoleApp && !IsDebuggerPresent() && !::hook) {
        hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)ShellProc, module, 0L);
    }
}

void removeHook() {
    if (::hook) {
        UnhookWindowsHookEx(::hook);
        ::hook = nullptr;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            ::module = module;
            break;

        case DLL_PROCESS_DETACH:
            ::module = nullptr;
            removeHook();
            break;
    }

    return TRUE;
}

class MMShellHook:
    public musik::core::sdk::IPlugin,
    public musik::core::sdk::IPlaybackRemote {
        public:
            virtual void Release() {
            }

            virtual const char* Name() {
                return "win32globalhotkeys";
            }

            virtual const char* Version() {
                return "0.3.0";
            }

            virtual const char* Author() {
                return "clangen";
            }

            virtual const char* Guid() {
                return "e2678930-ecd4-43b8-85e0-e41d634445b2";
            }

            virtual bool Configurable() {
                return false;
            }

            virtual void Configure() {
            }

            virtual void Reload() {
            }

            virtual int SdkVersion() {
                return musik::core::sdk::SdkVersion;
            }

            virtual void SetPlaybackService(musik::core::sdk::IPlaybackService* playback) {
                ::playback = playback;
                if (playback) {
                    installHook();
                }
                else {
                    removeHook();
                }
            }

            virtual void OnTrackChanged(musik::core::sdk::ITrack* track) {

            }

            virtual void OnPlaybackStateChanged(musik::core::sdk::PlaybackState state) {

            }

            virtual void OnPlaybackTimeChanged(double time) {

            }

            virtual void OnVolumeChanged(double volume) {

            }

            virtual void OnModeChanged(musik::core::sdk::RepeatMode repeatMode, bool shuffled) {

            }

            virtual void OnPlayQueueChanged() {

            }
};

static MMShellHook plugin;

extern "C" __declspec(dllexport) musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" __declspec(dllexport) musik::core::sdk::IPlaybackRemote* GetPlaybackRemote() {
    return &plugin;
}