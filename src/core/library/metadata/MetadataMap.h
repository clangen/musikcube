//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/sdk/IMetadataMap.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace musik { namespace core {

    class MetadataMap :
        public musik::core::sdk::IMetadataMap,
        public std::enable_shared_from_this<MetadataMap>
    {
        public:
            MetadataMap(
                unsigned long long id,
                const std::string& description,
                const std::string& type);

            virtual ~MetadataMap();

            /* IMetadataMap */
            virtual void Release();
            virtual unsigned long long GetId();
            virtual const char* GetDescription();
            virtual const char* GetType();

            virtual int GetValue(const char* key, char* dst, int size);
            virtual unsigned long long GetUint64(const char* key, unsigned long long defaultValue = 0ULL);
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL);
            virtual unsigned int GetUint32(const char* key, unsigned long defaultValue = 0);
            virtual int GetInt32(const char* key, unsigned int defaultValue = 0);
            virtual double GetDouble(const char* key, double defaultValue = 0.0f);

            /* implementation specific */
            void SetValue(const char* key, const std::string& value);
            std::string GetValue(const char* key);
            musik::core::sdk::IMetadataMap* GetSdkValue();

        private:
            unsigned long long id;
            std::string type, description;
            std::unordered_map<std::string, std::string> metadata;
    };

    using MetadataMapPtr = std::shared_ptr<MetadataMap>;

} }
