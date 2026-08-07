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

/** @file Outputs.h
 *  @brief Accessors for the audio output devices registered by plugins.
 *  @details Provides lookup of available IOutput devices, the currently selected
 *      output, and helpers to change the selection. */

#include <musikcore/config.h>
#include <musikcore/sdk/IOutput.h>

/** @namespace musik::core::audio::outputs
 *  @brief Utilities for enumerating and selecting audio output devices. */
namespace musik { namespace core { namespace audio { namespace outputs {

    using IOutput = musik::core::sdk::IOutput; /**< Output device alias. */

    /** @return All registered output devices. */
    std::vector<std::shared_ptr<IOutput>> GetAllOutputs();
    /** @return The number of registered output devices. */
    size_t GetOutputCount();
    /** @return The unmanaged output at the given index.
     *  @param index Zero-based output index. */
    IOutput* GetUnmanagedOutput(size_t index);
    /** @return The unmanaged output with the given name, or nullptr.
     *  @param name The output name to look up. */
    IOutput* GetUnmanagedOutput(const std::string& name);
    /** @return The currently selected unmanaged output, or nullptr. */
    IOutput* GetUnmanagedSelectedOutput();
    /** @return A shared pointer to the currently selected output. */
    std::shared_ptr<IOutput> SelectedOutput();

    /** @brief Selects an output device by shared pointer.
     *  @param output The output to select. */
    void SelectOutput(std::shared_ptr<IOutput> output);
    /** @brief Selects an output device by raw pointer.
     *  @param output The output to select. */
    void SelectOutput(IOutput* output);

} } } }
