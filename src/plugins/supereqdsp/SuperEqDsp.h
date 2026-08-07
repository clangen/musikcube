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
////////////////////////////////////////////////////////////////////////////

#pragma once

/// @file SuperEqDsp.h
/// @brief 31-band graphic equalizer DSP plugin.
/// @details Wraps the SuperEQ FFT-based equalizer engine from the DeaDBeeF
/// player. Processes PCM buffers through the equalizer when enabled and
/// re-reads the equalizer settings whenever they change (e.g. from the
/// equalizer overlay).

#include <musikcore/sdk/IDSP.h>
#include "supereq/Equ.h"

using namespace musik::core::sdk;

/** @brief 31-band graphic equalizer implemented with SuperEQ.
 *  @details Holds a SuperEqState and applies the configured band gains to each
 *  PCM buffer. The plugin-level NotifyChanged() signals the singleton that the
 *  settings changed so the filter table is rebuilt lazily. */
class SuperEqDsp : public IDSP {
    public:
        /** @brief Constructs a DSP with the equalizer disabled. */
        SuperEqDsp();
        /** @brief Destroys the DSP and frees the equalizer state. */
        ~SuperEqDsp();

        /** @brief Destroys the DSP instance. */
        virtual void Release() override;
        /** @brief Processes a PCM buffer through the equalizer.
         *  @param buffer The buffer to process in place.
         *  @return True if processing was performed. */
        virtual bool Process(IBuffer *buffer) override;

        /** @brief Marks the equalizer settings as changed.
         *  @details Instances pick up the change on their next Process call. */
        static void NotifyChanged();

    private:
        /** @brief The SuperEQ processing state. */
        SuperEqState* supereq {nullptr};
        /** @brief Timestamp of the last settings refresh. */
        int lastUpdated {0};
        /** @brief Whether the equalizer is enabled. */
        bool enabled;
};
