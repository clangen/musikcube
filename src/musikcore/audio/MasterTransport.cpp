//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "MasterTransport.h"

#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/support/Settings.h>
#include <musikcore/support/SettingKeys.h>

using namespace musik::core::audio;
using namespace musik::core;
using namespace musik::core::prefs;
using namespace musik::core::sdk;

MasterTransport::MasterTransport()
: prefs(Settings::ForComponent(components::Playback)) {
    this->type = static_cast<Type>(this->prefs->GetInt(
        keys::Transport, static_cast<int>(Type::Gapless)));

    this->SwitchTo(this->type);
}

MasterTransport::~MasterTransport() {
    this->transportConnections = {};
    this->transport.reset();
}

void MasterTransport::SwitchTo(Type type) {
    if (!this->transport || this->type != type) {
        this->type = type;
        this->prefs->SetInt(keys::Transport, static_cast<int>(this->type));

        const double volume = this->transport ? this->transport->Volume() : -1;

        this->transportConnections = {};
        if (this->transport) {
            this->transport->Destroy();
        }
        switch (this->type) {
            case Type::Gapless:
                this->transport = std::make_shared<GaplessTransport>();
                break;

            case Type::Crossfade:
                this->transport = std::make_shared<CrossfadeTransport>();
                break;
        }

        if (volume > 0) {
            this->transport->SetVolume(volume);
        }

        this->transportConnections = {
            this->transport->StreamEvent.Subscribe([this](auto state, std::string url) {
                this->OnStreamEvent(state, std::move(url));
            }),
            this->transport->PlaybackEvent.Subscribe([this](auto state) { this->OnPlaybackEvent(state); }),
            this->transport->VolumeChanged.Subscribe([this] { this->OnVolumeChanged(); }),
            this->transport->TimeChanged.Subscribe([this](double time) { this->OnTimeChanged(time); })
        };
    }
}

MasterTransport::Type MasterTransport::GetType() noexcept {
    return this->type;
}

void MasterTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    this->transport->PrepareNextTrack(uri, gain);
}

void MasterTransport::Start(const std::string& uri, Gain gain, StartMode type) {
    this->transport->Start(uri, gain, type);
}

std::string MasterTransport::Uri() {
    return this->transport->Uri();
}

void MasterTransport::StopForReplacement() {
    this->transport->StopForReplacement();
}

void MasterTransport::Stop() {
    this->transport->Stop();
}

void MasterTransport::Destroy() {
    this->transport->Destroy();
}

mc_result MasterTransport::Pause() {
    return this->transport->Pause();
}

mc_result MasterTransport::Resume() {
    return this->transport->Resume();
}

double MasterTransport::Position() {
    return this->transport->Position();
}

void MasterTransport::SetPosition(double seconds) {
    this->transport->SetPosition(seconds);
}

double MasterTransport::Volume() {
    return this->transport->Volume();
}

void MasterTransport::SetVolume(double volume) {
    this->transport->SetVolume(volume);
}

double MasterTransport::GetDuration() {
    return this->transport->GetDuration();
}

bool MasterTransport::IsMuted() {
    return this->transport->IsMuted();
}

void MasterTransport::SetMuted(bool muted) {
    this->transport->SetMuted(muted);
}

void MasterTransport::ReloadOutput() {
    this->transport->ReloadOutput();
}

PlaybackState MasterTransport::GetPlaybackState() {
    return this->transport->GetPlaybackState();
}

StreamState MasterTransport::GetStreamState() {
    return this->transport->GetStreamState();
}

void MasterTransport::OnStreamEvent(StreamState type, std::string url) {
    this->StreamEvent.Publish(type, url);
}

void MasterTransport::OnPlaybackEvent(PlaybackState type) {
    this->PlaybackEvent.Publish(type);
}

void MasterTransport::OnVolumeChanged() {
    this->VolumeChanged.Publish();
}

void MasterTransport::OnTimeChanged(double time) {
    this->TimeChanged.Publish(time);
}