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

#pragma once

#include <musikcore/plugin/audio.h>

/* internal playback-facing helpers for the runtime's selected visualizer and
shared spectrum meter. the runtime owns discovery, selection, and provider
lifetimes in Visualizer.cpp; output visualization workers deliver borrowed
audio here.

this is distinct from include/musikcore/visualizer.h, which exposes controls
to applications, and include/musikcore/plugin/visualizer.h, which defines the
C ABI implemented and registered by visualizer plugins. */
namespace musik { namespace core { namespace audio { namespace vis {

    /* check before copying output PCM for visualization. true when a visible
    plugin or the application spectrum meter needs audio; false lets playback
    skip capture, scheduling, and FFT work entirely. call on a worker, not an
    audio callback. a plugin may close its own window between checks. */
    bool HasAudioConsumers();

    /* deliver an already synchronized PCM chunk from an output visualization
    worker. forwards borrowed samples to the selected plugin and computes FFT
    bins only for consumers that need them: the plugin and/or the application
    spectrum meter. does nothing if all consumers have become inactive. */
    void DeliverAudio(const mc_pcm_view& pcm);

    /* report a skipped visualization capture without changing accepted audio.
    the audio queue calls this at most once per reset after an allocation or
    worker-start failure. diagnostic allocation failures are contained. */
    void ReportCaptureFailure(mc_result result) noexcept;

    /* hide and destroy plugin instances and disable spectrum capture. the
    runtime calls this after stopping/joining every output visualization worker,
    before unloading plugins; the next session can rediscover them. */
    void Shutdown();

} } } }
