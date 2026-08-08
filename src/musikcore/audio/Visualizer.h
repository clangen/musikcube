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

/** @file Visualizer.h
 *  @brief Registry and lifecycle for audio spectrum/PCM visualizers.
 *  @details Exposes the built-in spectrum and PCM visualizers plus helpers to
 *      enumerate, select and shut down visualizers. */

#include <musikcore/config.h>
#include <musikcore/sdk/ISpectrumVisualizer.h>
#include <musikcore/sdk/IPcmVisualizer.h>

/** @namespace musik::core::audio::vis
 *  @brief Visualizer registry and selection helpers. */
namespace musik { namespace core { namespace audio { namespace vis {

    /** @return The built-in spectrum analyzer visualizer instance. */
    musik::core::sdk::ISpectrumVisualizer* SpectrumVisualizer();
    /** @return The built-in PCM (waveform) visualizer instance. */
    musik::core::sdk::IPcmVisualizer* PcmVisualizer();

    /** @return The visualizer at the given index.
     *  @param index Zero-based visualizer index. */
    std::shared_ptr<musik::core::sdk::IVisualizer> GetVisualizer(size_t index);
    /** @return The total number of registered visualizers. */
    size_t VisualizerCount();
    /** @brief Selects the visualizer to render during playback.
     *  @param visualizer The visualizer to select. */
    void SetSelectedVisualizer(std::shared_ptr<musik::core::sdk::IVisualizer> visualizer);
    /** @return The currently selected visualizer, or nullptr. */
    std::shared_ptr<musik::core::sdk::IVisualizer> SelectedVisualizer();
    /** @brief Deactivates the selected visualizer without changing the selection. */
    void HideSelectedVisualizer();
    /** @brief Releases all visualizer resources (called on application shutdown). */
    void Shutdown();

} } } }
