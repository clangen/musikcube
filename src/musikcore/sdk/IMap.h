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

/** @file IMap.h @brief Defines the IMap interface for reading typed values from a key/value resource. */
#pragma once

#include "IValue.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A typed, key/value resource that exposes values as strings,
     *  integers, or doubles for a given key. */
    class IMap : public IValue {
        public:
            /** @brief Retrieves a value as a string.
             *  @param key The key to look up.
             *  @param dst The destination buffer for the value.
             *  @param size The capacity of the destination buffer.
             *  @return The number of bytes written, or the required size if the buffer was too small. */
            virtual int GetString(const char* key, char* dst, int size) = 0;

            /** @brief Retrieves a value as a 64-bit integer.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL) = 0;

            /** @brief Retrieves a value as a 32-bit integer.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual int GetInt32(const char* key, unsigned int defaultValue = 0) = 0;

            /** @brief Retrieves a value as a double.
             *  @param key The key to look up.
             *  @param defaultValue The value to return if the key is missing.
             *  @return The value, or the default. */
            virtual double GetDouble(const char* key, double defaultValue = 0.0f) = 0;
    };

} } }

