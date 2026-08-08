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

/** @file MetadataMap.h
 *  @brief IMap implementation holding metadata for a library entity.
 *  @details Wraps an unordered string->string map plus the entity's id, value and
 *      type so it can be exposed through the SDK IMap interface. Used to return
 *      album/category metadata to plugins and remote clients. */

#include <musikcore/sdk/IMap.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief A metadata record exposing a string map via the SDK IMap interface.
     *  @details Each map represents one entity (e.g. an album) with its own id,
     *      value and type. Arbitrary key/value pairs can be added with Set() and
     *      read back through GetString()/GetInt64()/etc. */
    class MetadataMap :
        public musik::core::sdk::IMap,
        public std::enable_shared_from_this<MetadataMap>
    {
        public:
            /** @brief Creates a metadata map for an entity.
             *  @param id The entity's unique id.
             *  @param value The entity's primary value (e.g. album name).
             *  @param type The entity's type (e.g. "album"). */
            MetadataMap(
                int64_t id,
                const std::string& value,
                const std::string& type);

            virtual ~MetadataMap();

            /* IResource */
            /** @return The entity id. */
            virtual int64_t GetId();
            /** @return The SDK resource class (IMap). */
            virtual musik::core::sdk::IResource::Class GetClass();
            /** @return The entity type string. */
            virtual const char* GetType();

            /* IValue */
            /** @brief Copies the primary value into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied. */
            virtual size_t GetValue(char* dst, size_t size);

            /* IMap */
            /** @brief Frees the map (deletes this instance). */
            virtual void Release();
            /** @brief Reads a string value by key.
             *  @param key The metadata key.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst.
             *  @return The number of bytes copied, or -1 if the key is missing. */
            virtual int GetString(const char* key, char* dst, int size);
            /** @brief Reads a 64-bit integer value by key.
             *  @param key The metadata key.
             *  @param defaultValue Value returned if the key is missing.
             *  @return The value. */
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL);
            /** @brief Reads a 32-bit integer value by key.
             *  @param key The metadata key.
             *  @param defaultValue Value returned if the key is missing.
             *  @return The value. */
            virtual int GetInt32(const char* key, unsigned int defaultValue = 0);
            /** @brief Reads a double value by key.
             *  @param key The metadata key.
             *  @param defaultValue Value returned if the key is missing.
             *  @return The value. */
            virtual double GetDouble(const char* key, double defaultValue = 0.0f);

            /* implementation specific */
            /** @brief Sets a metadata value by key.
             *  @param key The metadata key.
             *  @param value The value to store. */
            void Set(const char* key, const std::string& value);
            /** @brief Returns a metadata value by key.
             *  @param key The metadata key.
             *  @return The value, or an empty string if missing. */
            std::string Get(const char* key);
            /** @return The primary value of the map. */
            std::string GetTypeValue();
            /** @return A raw SDK IMap pointer (borrowed). */
            musik::core::sdk::IMap* GetSdkValue();
            /** @brief Iterates over all key/value pairs.
             *  @param callback Invoked for each pair. */
            void Each(std::function<void(const std::string&, const std::string&)> callback);

        private:
            int64_t id; /**< Entity id. */
            std::string type, value; /**< Entity type and primary value. */
            std::unordered_map<std::string, std::string> metadata; /**< Key/value metadata. */
    };

    using MetadataMapPtr = std::shared_ptr<MetadataMap>; /**< Shared metadata map alias. */

} }
