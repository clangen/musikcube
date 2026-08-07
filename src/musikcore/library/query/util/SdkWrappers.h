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

#pragma once

/** @file SdkWrappers.h
 *  @brief Adapter classes exposing query results through SDK interfaces.
 *  @details Wraps internal data types (values, value lists and track lists) so
 *      they can be returned to plugins and remote clients through the SDK
 *      IValue, IValueList and ITrackList interfaces. */

#include <musikcore/sdk/IValueList.h>
#include <musikcore/sdk/ITrackList.h>
#include <musikcore/support/Common.h>
#include <musikcore/library/track/TrackList.h>
#include <vector>
#include <memory>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief A single value exposed through the SDK IValue interface.
     *  @details Carries a display string, an id and a type. Lightweight and
     *      copyable; the SDK Release() is a no-op since these are shared. */
    class SdkValue: public musik::core::sdk::IValue {
        public:
            using Shared = std::shared_ptr<SdkValue>; /**< Shared value alias. */

            /** @brief Creates a value.
             *  @param displayValue The display text.
             *  @param id The value's id.
             *  @param type The value's type. */
            SdkValue(
                const std::string& displayValue,
                int64_t id,
                const std::string& type)
            {
                this->displayValue = displayValue;
                this->id = id;
                this->type = type;
            }

            /** @return The value's id. */
            virtual int64_t GetId() {
                return this->id;
            }

            /** @return IResource::Class::Value. */
            virtual musik::core::sdk::IResource::Class GetClass() {
                return musik::core::sdk::IResource::Class::Value;
            }

            /** @return The value's type string. */
            virtual const char* GetType() {
                return this->type.c_str();
            }

            /** @brief Copies the display value into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied. */
            virtual size_t GetValue(char* dst, size_t size) {
                return musik::core::CopyString(this->displayValue, dst, size);
            }

            /** @return The display value as a std::string. */
            std::string ToString() {
                return this->displayValue;
            }

            /** @brief No-op (values are shared and not manually freed). */
            virtual void Release() {
            }

        private:
            std::string displayValue; /**< Display text. */
            std::string type;         /**< Value type. */
            int64_t id;               /**< Value id. */
    };

    /** @brief A list of SdkValue entries exposed through IValueList.
     *  @details Stores a shared vector of SdkValue::Shared. Provides convenience
     *      helpers such as Filter(), Map() and Each() for working with results. */
    class SdkValueList : public musik::core::sdk::IValueList {
        public:
            using SharedValueList = std::shared_ptr<std::vector<SdkValue::Shared>>; /**< Shared vector alias. */
            using Shared = std::shared_ptr<SdkValueList>; /**< Shared list alias. */

            /** @brief Creates an empty value list. */
            SdkValueList() {
                values.reset(new std::vector<SdkValue::Shared>());
            }

            /** @brief Copy constructor (shares the underlying vector).
             *  @param other The list to copy. */
            SdkValueList(const SdkValueList& other) {
                this->values = other.values;
            }

            /** @brief Constructs from a shared list (shares the underlying vector).
             *  @param other The list to share. */
            SdkValueList(std::shared_ptr<SdkValueList>& other) {
                this->values = other->values;
            }

            /** @brief Constructs from an existing shared vector.
             *  @param values The vector to adopt. */
            SdkValueList(SharedValueList values) {
                this->values = values;
            }

            /** @brief Frees the list (deletes this instance). */
            virtual void Release() {
                delete this;
            }

            /** @return The number of values in the list. */
            virtual size_t Count() {
                return this->values->size();
            }

            /** @return The value at the given index, or nullptr.
             *  @param index Zero-based index. */
            virtual musik::core::sdk::IValue* GetAt(size_t index) {
                return this->values->at(index).get();
            }

            /** @return The shared value at the given index.
             *  @param index Zero-based index. */
            SdkValue::Shared At(size_t index) {
                return this->values->at(index);
            }

            /** @return The shared value at the given index.
             *  @param index Zero-based index. */
            SdkValue::Shared operator[](size_t index) {
                return this->values->at(index);
            }

            /** @brief Appends a value to the list.
             *  @param value The value to add. */
            void Add(std::shared_ptr<SdkValue> value) {
                this->values->push_back(value);
            }

            /** @brief Removes all values from the list. */
            void Clear() {
                this->values->clear();
            }

            /** @brief Sorts the list using the given comparator.
             *  @param compare The comparison function. */
            void Sort(std::function<bool(const SdkValue::Shared&, const SdkValue::Shared&)> compare) {
                std::sort(values->begin(), values->end(), compare);
            }

            /** @return A new list containing only values that pass the predicate.
             *  @param keep The keep predicate. */
            Shared Filter(std::function<bool(const SdkValue::Shared&)> keep) const {
                Shared result = std::make_shared<SdkValueList>();
                for (size_t i = 0; i < values->size(); i++) {
                    SdkValue::Shared value = values->at(i);
                    if (keep(value)) {
                        result->Add(value);
                    }
                }
                return result;
            }

            /** @brief Transforms each value into a new collection.
             *  @tparam T The output element type.
             *  @param fun The mapping function.
             *  @return The mapped results. */
            template <typename T>
            std::vector<T> Map(std::function<T(const SdkValue::Shared&)> fun) const {
                std::vector<T> result;
                for (size_t i = 0; i < values->size(); i++) {
                    result.push_back(fun(values->at(i)));
                }
                return result;
            }

            /** @brief Invokes a function for each value.
             *  @param fun The callback. */
            void Each(std::function<void(const SdkValue::Shared&)> fun) const {
                for (size_t i = 0; i < values->size(); i++) {
                    fun(values->at(i));
                }
            }

        private:
            SharedValueList values; /**< Shared underlying vector. */
    };

    /** @brief Adapts an internal TrackList to the SDK ITrackList interface.
     *  @details Wraps a shared TrackList and forwards count, id and track lookups
     *      to it. Release() deletes the wrapper (not the underlying list). */
    class SdkTrackList : public musik::core::sdk::ITrackList {
        public:
            /** @brief Wraps an existing track list.
             *  @param wrapped The internal track list to adapt. */
            SdkTrackList(std::shared_ptr<musik::core::TrackList> wrapped) {
                this->wrapped = wrapped;
            }

            virtual ~SdkTrackList() {
            }

            /** @brief Frees the wrapper (deletes this instance). */
            virtual void Release() override {
                delete this;
            }

            /** @return The number of tracks in the list. */
            virtual size_t Count() const override {
                return this->wrapped->Count();
            }

            /** @return The id of the track at the given index.
             *  @param index Zero-based index. */
            virtual int64_t GetId(size_t index) const override {
                return this->wrapped->GetId(index);
            }

            /** @return The index of the track with the given id, or -1.
             *  @param id The track id. */
            virtual int IndexOf(int64_t id) const override {
                return this->wrapped->IndexOf(id);
            }

            /** @return The track at the given index, or nullptr.
             *  @param index Zero-based index. */
            virtual musik::core::sdk::ITrack* GetTrack(size_t index) const override {
                return this->wrapped->GetTrack(index);
            }

        private:
            std::shared_ptr<musik::core::TrackList> wrapped; /**< Wrapped internal track list. */
    };

} } } }