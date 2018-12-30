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

#pragma once

#include <core/sdk/IValueList.h>
#include <core/sdk/ITrackList.h>
#include <core/support/Common.h>
#include <core/library/track/TrackList.h>
#include <vector>
#include <memory>

namespace musik { namespace core { namespace db { namespace local {

    class SdkValue: public musik::core::sdk::IValue {
        public:
            using Shared = std::shared_ptr<SdkValue>;

            SdkValue(
                const std::string& displayValue,
                int64_t id,
                const std::string& type)
            {
                this->displayValue = displayValue;
                this->id = id;
                this->type = type;
            }

            virtual int64_t GetId() {
                return this->id;
            }

            virtual musik::core::sdk::IResource::Class GetClass() {
                return musik::core::sdk::IResource::Class::Value;
            }

            virtual const char* GetType() {
                return this->type.c_str();
            }

            virtual size_t GetValue(char* dst, size_t size) {
                return musik::core::CopyString(this->displayValue, dst, size);
            }

            std::string ToString() {
                return this->displayValue;
            }

            virtual void Release() {
            }

        private:
            std::string displayValue;
            std::string type;
            int64_t id;
    };

    class SdkValueList : public musik::core::sdk::IValueList {
        public:
            using SharedValueList = std::shared_ptr<std::vector<SdkValue::Shared>>;
            using Shared = std::shared_ptr<SdkValueList>;

            SdkValueList() {
                values.reset(new std::vector<SdkValue::Shared>());
            }

            SdkValueList(const SdkValueList& other) {
                this->values = other.values;
            }

            SdkValueList(std::shared_ptr<SdkValueList>& other) {
                this->values = other->values;
            }

            SdkValueList(SharedValueList values) {
                this->values = values;
            }

            virtual void Release() {
                delete this;
            }

            virtual size_t Count() {
                return this->values->size();
            }

            virtual musik::core::sdk::IValue* GetAt(size_t index) {
                return this->values->at(index).get();
            }

            SdkValue::Shared At(size_t index) {
                return this->values->at(index);
            }

            SdkValue::Shared operator[](size_t index) {
                return this->values->at(index);
            }

            void Add(std::shared_ptr<SdkValue> value) {
                this->values->push_back(value);
            }

            void Sort(std::function<bool(const SdkValue::Shared&, const SdkValue::Shared&)> compare) {
                std::sort(values->begin(), values->end(), compare);
            }

            Shared Filter(std::function<bool(const SdkValue::Shared&)> keep) {
                Shared result = std::make_shared<SdkValueList>();
                for (size_t i = 0; i < values->size(); i++) {
                    SdkValue::Shared value = values->at(i);
                    if (keep(value)) {
                        result->Add(value);
                    }
                }
                return result;
            }

            template <typename T>
            std::vector<T> Map(std::function<T(const SdkValue::Shared&)> fun) {
                std::vector<T> result;
                for (size_t i = 0; i < values->size(); i++) {
                    result.push_back(fun(values->at(i)));
                }
                return result;
            }

            void Each(std::function<void(const SdkValue::Shared&)> fun) {
                for (size_t i = 0; i < values->size(); i++) {
                    fun(values->at(i));
                }
            }

        private:
            SharedValueList values;
    };

    class SdkTrackList : public musik::core::sdk::ITrackList {
        public:
            SdkTrackList(std::shared_ptr<musik::core::TrackList> wrapped) {
                this->wrapped = wrapped;
            }

            virtual ~SdkTrackList() {
            }

            virtual void Release() override {
                delete this;
            }

            virtual size_t Count() const override {
                return this->wrapped->Count();
            }

            virtual int64_t GetId(size_t index) const override {
                return this->wrapped->GetId(index);
            }

            virtual int IndexOf(int64_t id) const override {
                return this->wrapped->IndexOf(id);
            }

            virtual musik::core::sdk::ITrack* GetTrack(size_t index) const override {
                return this->wrapped->GetTrack(index);
            }

        private:
            std::shared_ptr<musik::core::TrackList> wrapped;
    };

} } } }