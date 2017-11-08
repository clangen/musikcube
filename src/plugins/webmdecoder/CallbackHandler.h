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

#pragma once

#include <string>
#include <webm/callback.h>

class CallbackHandler : public webm::Callback {
    public:
        CallbackHandler();
        double GetDuration() { return duration; };
        long GetSampleRate() { return sample_rate; };
        int GetChannels() { return channels; };
        webm::Status OnElementBegin(const webm::ElementMetadata& metadata,
                                    webm::Action* action) override;
        webm::Status OnUnknownElement(const webm::ElementMetadata& metadata,
                                      webm::Reader* reader,
                                      std::uint64_t* bytes_remaining) override;
        webm::Status OnEbml(const webm::ElementMetadata& metadata,
                            const webm::Ebml& ebml) override;
        webm::Status OnVoid(const webm::ElementMetadata& metadata,
                            webm::Reader* reader,
                            std::uint64_t* bytes_remaining) override;
        webm::Status OnSegmentBegin(const webm::ElementMetadata& metadata,
                                    webm::Action* action) override;
        webm::Status OnSeek(const webm::ElementMetadata& metadata,
                            const webm::Seek& seek) override;
        webm::Status OnInfo(const webm::ElementMetadata& metadata,
                            const webm::Info& info) override;
        webm::Status OnClusterBegin(const webm::ElementMetadata& metadata,
                                    const webm::Cluster& cluster,
                                    webm::Action* action) override;
        webm::Status OnSimpleBlockBegin(const webm::ElementMetadata& metadata,
                                        const webm::SimpleBlock& simple_block,
                                        webm::Action* action) override;
        webm::Status OnSimpleBlockEnd(const webm::ElementMetadata& metadata,
                                      const webm::SimpleBlock& simple_block) override;
        webm::Status OnBlockGroupBegin(const webm::ElementMetadata& metadata,
                                       webm::Action* action) override;
        webm::Status OnBlockBegin(const webm::ElementMetadata& metadata,
                                  const webm::Block& block,
                                  webm::Action* action) override;
        webm::Status OnBlockEnd(const webm::ElementMetadata& metadata,
                                const webm::Block& block) override;
        webm::Status OnBlockGroupEnd(const webm::ElementMetadata& metadata,
                                     const webm::BlockGroup& block_group) override;
        webm::Status OnFrame(const webm::FrameMetadata& metadata,
                             webm::Reader* reader,
                             std::uint64_t* bytes_remaining) override;
        webm::Status OnClusterEnd(const webm::ElementMetadata& metadata,
                                  const webm::Cluster& cluster) override;
        webm::Status OnTrackEntry(const webm::ElementMetadata& metadata,
                                  const webm::TrackEntry& track_entry) override;
        webm::Status OnCuePoint(const webm::ElementMetadata& metadata,
                                const webm::CuePoint& cue_point) override;
        webm::Status OnEditionEntry(const webm::ElementMetadata& metadata,
                                    const webm::EditionEntry& edition_entry) override;
        webm::Status OnTag(const webm::ElementMetadata& metadata,
                           const webm::Tag& tag) override;
        webm::Status OnSegmentEnd(const webm::ElementMetadata& metadata) override;

    private:
        double duration;
        long sample_rate;
        int channels;
        void DumpMetadata(const webm::ElementMetadata& metadata);
        void DumpMetadata(const webm::FrameMetadata& metadata);
};

