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

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/sdk/IMetadataValueList.h>
#include <core/support/Common.h>
#include <memory>

namespace musik { namespace core { namespace db { namespace local {

    class CategoryListQuery : public musik::core::db::QueryBase<musik::core::db::Connection> {
        public:
            /* note we implement the SDK's IMetadataValue interface so
            we can return data to plugins! */
            struct Result : public musik::core::sdk::IMetadataValue {
                virtual unsigned long long GetId() {
                    return this->id;
                }

                virtual const char* GetValue() {
                    return this->displayValue.c_str();
                }

                virtual int GetValue(char* dst, size_t size) {
                    return musik::core::CopyString(this->displayValue, dst, size);
                }

                std::string displayValue;
                DBID id;
            };

            typedef std::shared_ptr<std::vector<
                std::shared_ptr<Result> > > ResultList;

            CategoryListQuery(
                const std::string& trackField,
                const std::string& filter = "");

            virtual ~CategoryListQuery();

            std::string Name() { return "CategoryListQuery"; }

            virtual ResultList GetResult();
            virtual int GetIndexOf(DBID id);

            musik::core::sdk::IMetadataValueList* GetSdkResult();

        protected:
            virtual bool OnRun(musik::core::db::Connection &db);

            std::string trackField;
            std::string filter;
            ResultList result;
    };

} } } }
