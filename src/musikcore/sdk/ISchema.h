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

/** @file ISchema.h @brief Defines the ISchema interface and TSchema builder for typed plugin settings. */
#pragma once

#include <stddef.h>
#include <float.h>
#include <climits>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A read-only description of a plugin's configuration settings,
     *  used to drive generated configuration UIs. */
    class ISchema {
        public:
            /** @brief The data type of a schema entry. */
            enum class Type {
                /** @brief A boolean setting. */
                Bool, Int, Double, String, Enum
            };

            /** @brief A base entry describing a single setting. */
            struct Entry {
                /** @brief The data type of the entry. */
                Type type;
                /** @brief The name of the entry. */
                const char* name;
            };

            /** @brief A boolean setting entry. */
            struct BoolEntry {
                /** @brief The base entry data. */
                Entry entry;
                /** @brief The default value. */
                bool defaultValue;
            };

            /** @brief An integer setting entry. */
            struct IntEntry {
                /** @brief The base entry data. */
                Entry entry;
                /** @brief The minimum allowed value. */
                int minValue;
                /** @brief The maximum allowed value. */
                int maxValue;
                /** @brief The default value. */
                int defaultValue;
            };

            /** @brief A double precision setting entry. */
            struct DoubleEntry {
                /** @brief The base entry data. */
                Entry entry;
                /** @brief The minimum allowed value. */
                double minValue;
                /** @brief The maximum allowed value. */
                double maxValue;
                /** @brief The number of decimal places to display. */
                int precision;
                /** @brief The default value. */
                double defaultValue;
            };

            /** @brief A string setting entry. */
            struct StringEntry {
                /** @brief The base entry data. */
                Entry entry;
                /** @brief The default value. */
                const char* defaultValue;
            };

            /** @brief An enumerated setting entry. */
            struct EnumEntry {
                /** @brief The base entry data. */
                Entry entry;
                /** @brief The number of possible values. */
                size_t count;
                /** @brief The possible values. */
                const char** values;
                /** @brief The default value. */
                const char* defaultValue;
            };

            /** @brief Releases the schema; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns the number of entries in the schema.
             *  @return The entry count. */
            virtual size_t Count() = 0;

            /** @brief Returns the entry at the given index.
             *  @param index The zero-based index.
             *  @return The entry, or null if out of range. */
            virtual const Entry* At(size_t index) = 0;
    };

    /** @brief A concrete, builder-style implementation of ISchema that owns the
     *  memory of its entries and frees it when released.
     *  @tparam T The ISchema-derived interface to implement. */
    template <typename T = ISchema>
    class TSchema: public ISchema {
        public:
            /** @brief Destroys the schema, freeing all owned entry data. */
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

            /** @brief Releases the schema, deleting this instance. */
            virtual void Release() override {
                delete this;
            }

            /** @brief Returns the number of entries in the schema.
             *  @return The entry count. */
            virtual size_t Count() override {
                return entries.size();
            }

            /** @brief Returns the entry at the given index.
             *  @param index The zero-based index.
             *  @return The entry, or null if out of range. */
            virtual const Entry* At(size_t index) override {
                return entries[index];
            }

            /** @brief Adds a boolean setting to the schema.
             *  @param name The setting name.
             *  @param defaultValue The default value.
             *  @return This schema, for chaining. */
            TSchema& AddBool(const std::string& name, bool defaultValue) {
                auto entry = new BoolEntry();
                entry->entry.type = ISchema::Type::Bool;
                entry->entry.name = AllocString(name);
                entry->defaultValue = defaultValue;
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            /** @brief Adds an integer setting to the schema.
             *  @param name The setting name.
             *  @param defaultValue The default value.
             *  @param min The minimum allowed value.
             *  @param max The maximum allowed value.
             *  @return This schema, for chaining. */
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

            /** @brief Adds a double precision setting to the schema.
             *  @param name The setting name.
             *  @param defaultValue The default value.
             *  @param precision The number of decimal places to display.
             *  @param min The minimum allowed value.
             *  @param max The maximum allowed value.
             *  @return This schema, for chaining. */
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

            /** @brief Adds a string setting to the schema.
             *  @param name The setting name.
             *  @param defaultValue The default value.
             *  @return This schema, for chaining. */
            TSchema& AddString(const std::string& name, const std::string& defaultValue) {
                auto entry = new StringEntry();
                entry->entry.type = ISchema::Type::String;
                entry->entry.name = AllocString(name);
                entry->defaultValue = AllocString(defaultValue);
                entries.push_back(reinterpret_cast<Entry*>(entry));
                return *this;
            }

            /** @brief Adds an enumerated setting to the schema.
             *  @param name The setting name.
             *  @param values The possible values.
             *  @param defaultValue The default value.
             *  @return This schema, for chaining. */
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
            /** @brief Allocates a null-terminated copy of each value in the given vector.
             *  @param values The source values.
             *  @return A heap-allocated array of owned string pointers. */
            const char** AllocStringList(const std::vector<std::string>& values) {
                const char** result = new const char*[values.size()];
                for (size_t i = 0; i < values.size(); i++) {
                    result[i] = AllocString(values[i]);
                }
                return result;
            }

            /** @brief Frees a heap-allocated array of owned string pointers.
             *  @param values The array to free.
             *  @param count The number of elements in the array. */
            void FreeStringList(const char** values, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    FreeString(values[i]);
                }
                delete[] values;
            }

            /** @brief Allocates a null-terminated copy of the given string.
             *  @param str The source string.
             *  @return A heap-allocated owned string. */
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

            /** @brief Frees a heap-allocated owned string.
             *  @param str The string to free. */
            void FreeString(const char* str) {
                delete[] str;
            }

            /** @brief The owned entries managed by this schema. */
            std::vector<Entry*> entries;
    };

} } }

