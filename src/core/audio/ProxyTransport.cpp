//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <pch.hpp>

#include "ProxyTransport.h"

#include <core/audio/GaplessTransport.h>
#include <core/audio/CrossfadeTransport.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>

using namespace musik::core::audio;
using namespace musik::core;
using namespace musik::core::prefs;
using namespace musik::core::sdk;

ProxyTransport::ProxyTransport()
: prefs(Preferences::ForComponent(components::Playback)) {
    this->type = (Type) this->prefs->GetInt(keys::Transport, (int) Type::Gapless);
    this->SwitchTo(this->type);
}

ProxyTransport::~ProxyTransport() {

}

void ProxyTransport::SwitchTo(Type type) {
    if (!this->transport || this->type != type) {
        this->type = type;
        this->prefs->SetInt(keys::Transport, (int) this->type);

        double volume = this->transport ? this->transport->Volume() : -1;

        switch (this->type) {
            case Type::Gapless:
                if (this->transport) {
                    /* hacky -- we know it's a crossfade transport, stop it
                    immediately without fading out so we don't block the UI
                    for a second or so. */
                    static_cast<CrossfadeTransport*>
                        (this->transport.get())->StopImmediately();
                }

                this->transport.reset(new GaplessTransport());
                break;

            case Type::Crossfade:
                this->transport.reset(new CrossfadeTransport());
                break;
        }

        if (volume > 0) {
            this->transport->SetVolume(volume);
        }

        this->transport->PlaybackEvent.connect(
            this, &ProxyTransport::OnPlaybackEvent);

        this->transport->StreamEvent.connect(
            this, &ProxyTransport::OnStreamEvent);

        this->transport->TimeChanged.connect(
            this, &ProxyTransport::OnTimeChanged);

        this->transport->VolumeChanged.connect(
            this, &ProxyTransport::OnVolumeChanged);
    }
}

ProxyTransport::Type ProxyTransport::GetType() {
    return this->type;
}

void ProxyTransport::PrepareNextTrack(const std::string& trackUrl, Gain gain) {
    this->transport->PrepareNextTrack(trackUrl, gain);
}

void ProxyTransport::Start(const std::string& trackUrl, Gain gain) {
    this->transport->Start(trackUrl, gain);
}

void ProxyTransport::Stop() {
    this->transport->Stop();
}

bool ProxyTransport::Pause() {
    return this->transport->Pause();
}

bool ProxyTransport::Resume() {
    return this->transport->Resume();
}

double ProxyTransport::Position() {
    return this->transport->Position();
}

void ProxyTransport::SetPosition(double seconds) {
    this->transport->SetPosition(seconds);
}

double ProxyTransport::Volume() {
    return this->transport->Volume();
}

void ProxyTransport::SetVolume(double volume) {
    this->transport->SetVolume(volume);
}

double ProxyTransport::GetDuration() {
    return this->transport->GetDuration();
}

bool ProxyTransport::IsMuted() {
    return this->transport->IsMuted();
}

void ProxyTransport::SetMuted(bool muted) {
    this->transport->SetMuted(muted);
}

void ProxyTransport::ReloadOutput() {
    this->transport->ReloadOutput();
}

PlaybackState ProxyTransport::GetPlaybackState() {
    return this->transport->GetPlaybackState();
}

void ProxyTransport::OnStreamEvent(int type, std::string url) {
    this->StreamEvent(type, url);
}

void ProxyTransport::OnPlaybackEvent(int type) {
    this->PlaybackEvent(type);
}

void ProxyTransport::OnVolumeChanged() {
    this->VolumeChanged();
}

void ProxyTransport::OnTimeChanged(double time) {
    this->TimeChanged(time);
}