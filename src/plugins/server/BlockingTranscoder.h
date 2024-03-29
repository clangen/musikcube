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

#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IBlockingEncoder.h>
#include <musikcore/sdk/DataBuffer.h>
#include "Context.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <stdio.h>

class BlockingTranscoder {
    public:
        using PositionType = musik::core::sdk::PositionType;

        BlockingTranscoder(
            Context& context,
            musik::core::sdk::IBlockingEncoder* encoder,
            const std::string& uri,
            const std::string& tempFilename,
            const std::string& finalFilename,
            int bitrate);

        virtual ~BlockingTranscoder();

        bool Transcode();
        void Interrupt();

        static int GetActiveCount();

    private:
        void Cleanup();

        Context& context;
        musik::core::sdk::IDataStream* input;
        musik::core::sdk::IBlockingEncoder* encoder;
        musik::core::sdk::IDataStream* output;
        std::string tempFilename, finalFilename;
        int bitrate;
        bool interrupted;
};