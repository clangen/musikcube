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

#include "pch.hpp"
#include "MetadataMapList.h"

using namespace musik::core;
using namespace musik::core::sdk;

namespace {
    class SdkWrapper : public IMapList {
        public:
            SdkWrapper(MetadataMapListPtr wrapped) { this->wrapped = wrapped; }
            virtual void Release() { this->wrapped.reset(); }
            virtual size_t Count() const { return this->wrapped->Count(); }
            virtual IMap* GetAt(size_t index) const { return this->wrapped->GetAt(index); }
            MetadataMapListPtr wrapped;
    };
}

MetadataMapList::MetadataMapList() {

}

MetadataMapList::~MetadataMapList() {

}

void MetadataMapList::Release() {
    /* nothing. wrapper helps with cleanup. */
}

size_t MetadataMapList::Count() const {
    return this->entries.size();
}

IMap* MetadataMapList::GetAt(size_t index) const {
    return this->entries.at(index)->GetSdkValue();
}

void MetadataMapList::Add(MetadataMapPtr entry) {
    this->entries.push_back(entry);
}

IMapList* MetadataMapList::GetSdkValue() {
    return new SdkWrapper(shared_from_this());
}
