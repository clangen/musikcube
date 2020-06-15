//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "Utility.h"
#include "OpenMptDataStream.h"
#include <core/sdk/IIndexerSource.h>
#include <core/sdk/IEnvironment.h>
#include <core/sdk/IIndexerSource.h>

using namespace musik::core::sdk;

extern IEnvironment* environment;

bool OpenMptDataStream::Open(const char *uri, OpenFlags flags) {
    if (indexer::parseExternalId(EXTERNAL_ID_PREFIX, std::string(uri), this->filename, this->trackNumber)) {
        if (environment) {
            this->stream = environment->GetDataStream(this->filename.c_str(), flags);
            if (this->stream) {
                return true;
            }
        }
    }
    return false;
}

bool OpenMptDataStream::Close() {
    return this->stream->Close();
}

void OpenMptDataStream::Interrupt() {
    this->stream->Interrupt();
}

void OpenMptDataStream::Release() {
    if (stream) {
        stream->Release();
        stream = nullptr;
    }
    delete this;
}

PositionType OpenMptDataStream::Read(void *buffer, PositionType readBytes) {
    return this->stream->Read(buffer, readBytes);
}

bool OpenMptDataStream::SetPosition(PositionType position) {
    return this->stream->SetPosition(position);
}

PositionType OpenMptDataStream::Position() {
    return this->stream->Position();
}

bool OpenMptDataStream::Seekable() {
    return this->stream->Seekable();
}

bool OpenMptDataStream::Eof() {
    return this->stream->Eof();
}

long OpenMptDataStream::Length() {
    return this->stream->Length();
}

const char* OpenMptDataStream::Type() {
    return this->stream->Type();
}

const char* OpenMptDataStream::Uri() {
    return this->stream->Uri();
}

bool OpenMptDataStream::CanPrefetch() {
    return this->stream->CanPrefetch();
}
