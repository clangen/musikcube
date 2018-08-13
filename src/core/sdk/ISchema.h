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

#pragma once

#include <stddef.h>
#include <float.h>
#include <climits>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

namespace musik { namespace core { namespace sdk {

    class ISchema {
        public:
            enum class Type {
                Bool, Int, Double, String, Enum
            };

            struct Entry {
                Type type;
                const char* name;
            };

            struct BoolEntry {
                Entry entry;
                bool defaultValue;
            };

            struct IntEntry {
                Entry entry;
                int minValue;
                int maxValue;
                int defaultValue;
            };

            struct DoubleEntry {
                Entry entry;
                double minValue;
                double maxValue;
                int precision;
                double defaultValue;
            };

            struct StringEntry {
                Entry entry;
                const char* defaultValue;
            };

            struct EnumEntry {
                Entry entry;
                size_t count;
                const char** values;
                const char* defaultValue;
            };

            virtual void Release() = 0;
            virtual size_t Count() = 0;
            virtual const Entry* At(size_t index) = 0;
    };

    template <typename T = ISchema>
    class TSchema: public ISchema {
        public:
            virtual ~TSchema() {
                for (auto it : this->entries) {
                    switch (it->type) {
                        case Type::String: {
                            StringEntry* entry = reinterpret_cast<StringEntry*>(it);
                            FreeString(entry->defaultValue);
                            break;
                        }

                        case Type::Enum: {
                            EnumEntry* entry = reinterpret_cast<EnumEntry*>(it);
                            FreeString(entry->defaultValue);
                            FreeStringList(entry->values, entry->count);
                            break;
                        }

                        default:
                            break;
                    }

                    FreeString(it->name);
                    delete it;
                }
            }

            virtual void Release() override {
                delete this;
            }

            virtual size_t Count() override {
                return entries.size();
            }

            virtual const Entry* At(size_t index) override {
                return entries[index];
            }

            TSchema& AddBool(const std::string& name, bool defaultValue) {
                auto entry = new BoolEntry();
                entry->entry.type = ISchema::Type::Bool;
                entry->entry.name = AllocString(name);
                entry->defaultValue = defaultValue;
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            TSchema& AddInt(
                const std::string& name,
                int defaultValue,
                int min = INT_MIN,
                int max = INT_MAX)
            {
                auto entry = new IntEntry();
                entry->entry.type = ISchema::Type::Int;
                entry->entry.name = AllocString(name);
                entry->defaultValue = defaultValue;
                entry->minValue = min;
                entry->maxValue = max;
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            TSchema& AddDouble(
                const std::string& name,
                double defaultValue,
                int precision = 2,
                double min = DBL_MIN,
                double max = DBL_MAX)
            {
                auto entry = new DoubleEntry();
                entry->entry.type = ISchema::Type::Double;
                entry->entry.name = AllocString(name);
                entry->defaultValue = defaultValue;
                entry->precision = precision;
                entry->minValue = min;
                entry->maxValue = max;
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            TSchema& AddString(const std::string& name, const std::string& defaultValue) {
                auto entry = new StringEntry();
                entry->entry.type = ISchema::Type::String;
                entry->entry.name = AllocString(name);
                entry->defaultValue = AllocString(defaultValue);
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            TSchema& AddEnum(
                const std::string& name,
                const std::vector<std::string>&& values,
                const std::string& defaultValue)
            {
                auto entry = new EnumEntry();
                entry->entry.type = ISchema::Type::Enum;
                entry->entry.name = AllocString(name);
                entry->defaultValue = AllocString(defaultValue);
                entry->count = values.size();
                entry->values = AllocStringList(values);
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

        private:
            const char** AllocStringList(const std::vector<std::string>& values) {
                const char** result = new const char*[values.size()];
                for (size_t i = 0; i < values.size(); i++) {
                    result[i] = AllocString(values[i]);
                }
                return result;
            }

            void FreeStringList(const char** values, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    FreeString(values[i]);
                }
                delete[] values;
            }

            const char* AllocString(const std::string& str) {
                char* result = new char[str.size() + 1];
#ifdef WIN32
                strncpy_s(result, str.size() + 1, str.c_str(), str.size());
#else
                strncpy(result, str.c_str(), str.size());
#endif
                result[str.size()] = 0;
                return result;
            }

            void FreeString(const char* str) {
                delete[] str;
            }

            std::vector<Entry*> entries;
    };

} } }

