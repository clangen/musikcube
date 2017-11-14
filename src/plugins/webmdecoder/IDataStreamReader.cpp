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

#include <iostream>
#include "IDataStreamReader.h"

webm::Status IDataStreamReader::Read(size_t num_to_read,
                                     std::uint8_t* buffer,
                                     std::uint64_t* num_actually_read)
{
    musik::core::sdk::PositionType bytesRead = this->stream->Read((void*) buffer, (musik::core::sdk::PositionType) num_to_read);
    if (bytesRead <= 0) {
        *num_actually_read = 0;
        return webm::Status(webm::Status::kEndOfFile);
    } else {
        *num_actually_read = bytesRead;
        if (bytesRead < num_to_read) {
            return webm::Status(webm::Status::kOkPartial);
        } else {
            return webm::Status(webm::Status::kOkCompleted);
        }
    } 
}

webm::Status IDataStreamReader::Skip(std::uint64_t num_to_skip,
                                     std::uint64_t* num_actually_skipped)
{
    if (this->stream->Seekable()) {
        if (this->stream->SetPosition(this->stream->Position() + num_to_skip)) {
            *num_actually_skipped = num_to_skip;
            return webm::Status(webm::Status::kOkCompleted);
        }
        else {
            std::cerr << "IDataStreamReader:Seek failed" << std::endl;
        }
    }
    *num_actually_skipped = 0;
    /* Again, might not be EOF. Is just a failure code for now */
    std::cerr << "IDataStreamReader:Nothing skipped" << std::endl;
    return webm::Status(webm::Status::kEndOfFile);
}

std::uint64_t IDataStreamReader::Position() const
{
    return this->stream->Position();
}

