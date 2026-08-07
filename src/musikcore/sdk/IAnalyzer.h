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

/** @file IAnalyzer.h @brief Defines the IAnalyzer interface for extracting metadata from PCM audio. */
#pragma once

#include "ITagStore.h"
#include "IBuffer.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief Analyzes decoded PCM audio to derive metadata, such as ReplayGain
     *  values, which is written to an ITagStore. */
    class  IAnalyzer {
        public:
            /** @brief Releases the analyzer; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Begins an analysis session.
             *  @param target The tag store that analysis results are written to.
             *  @return True if analysis can begin. */
            virtual bool Start(musik::core::sdk::ITagStore *target) = 0;

            /** @brief Analyzes a single buffer of PCM audio.
             *  @param target The tag store that analysis results are written to.
             *  @param buffer The PCM audio data to analyze.
             *  @return True if the buffer was processed successfully. */
            virtual bool Analyze(musik::core::sdk::ITagStore *target, IBuffer *buffer) = 0;

            /** @brief Finishes the analysis session and commits any results.
             *  @param target The tag store that analysis results are written to.
             *  @return True if the session ended successfully. */
            virtual bool End(musik::core::sdk::ITagStore *target) = 0;
    };

} } }

