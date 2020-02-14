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
#include "MetadataMap.h"

#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/support/Common.h>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::sdk;

namespace {
    /* a wrapper around a shared pointer to a MetadataMap. we
    can pass this to a plugin and it will keep the instance
    around until it's released, even if the containing list is
    released. */
    class SdkWrapper : public IMap {
        public:
            SdkWrapper(MetadataMapPtr wrapped) { this->wrapped = wrapped; };
            virtual void Release() { this->wrapped.reset(); }
            virtual int64_t GetId() { return this->wrapped->GetId(); }
            virtual IResource::Class GetClass() { return this->wrapped->GetClass(); }
            virtual int GetString(const char* key, char* dst, int size) { return this->wrapped->GetString(key, dst, size); }
            virtual long long GetInt64(const char* key, long long defaultValue) { return this->wrapped->GetInt64(key, defaultValue); }
            virtual int GetInt32(const char* key, unsigned int defaultValue) { return this->wrapped->GetInt32(key, defaultValue); }
            virtual double GetDouble(const char* key, double defaultValue) { return this->wrapped->GetDouble(key, defaultValue); }
            virtual size_t GetValue(char* dst, size_t size) { return this->wrapped->GetValue(dst, size); }
            virtual const char* GetType() { return this->wrapped->GetType(); }
            MetadataMapPtr wrapped;
    };
}

MetadataMap::MetadataMap(
    int64_t id,
    const std::string& value,
    const std::string& type)
{
    this->id = id;
    this->value = value;
    this->type = type;
}

MetadataMap::~MetadataMap() {

}

void MetadataMap::Release() {
    /* nothing... */
}

int64_t MetadataMap::GetId() {
    return this->id;
}

musik::core::sdk::IResource::Class MetadataMap::GetClass() {
    return musik::core::sdk::IResource::Class::Map;
}

int MetadataMap::GetString(const char* key, char* dst, int size) {
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        return (int) CopyString(it->second, dst, (size_t) size);
    }

    if (dst && size > 0) {
        dst[0] = 0;
    }

    return 0;
}

std::string MetadataMap::GetValue(const char* key) {
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        return it->second;
    }
    return "";
}

long long MetadataMap::GetInt64(const char* key, long long defaultValue) {
    try {
        std::string value = GetValue(key);
        if (value.size()) {
            return std::stoll(GetValue(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

int MetadataMap::GetInt32(const char* key, unsigned int defaultValue) {
    try {
        std::string value = GetValue(key);
        if (value.size()) {
            return std::stol(GetValue(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

double MetadataMap::GetDouble(const char* key, double defaultValue) {
    try {
        std::string value = GetValue(key);
        if (value.size()) {
            return std::stod(GetValue(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

size_t MetadataMap::GetValue(char* dst, size_t size) {
    return CopyString(this->value, dst, size);
}

const char* MetadataMap::GetType() {
    return this->type.c_str();
}

void MetadataMap::SetValue(const char* key, const std::string& value) {
    this->metadata[key] = value;
}

musik::core::sdk::IMap* MetadataMap::GetSdkValue() {
    return new SdkWrapper(shared_from_this());
}