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

/** @file MetadataMapList.h
 *  @brief IMapList implementation holding a list of MetadataMap entries.
 *  @details Stores MetadataMapPtr entries in a vector and exposes them through the
 *      SDK IMapList interface, used to return collections of metadata to plugins
 *      and remote clients. */

#include <musikcore/sdk/IMapList.h>
#include "MetadataMap.h"
#include <vector>
#include <memory>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief An ordered collection of metadata maps.
     *  @details Supports appending MetadataMapPtr entries, clearing the list, and
     *      reading entries by index either as shared pointers or as SDK pointers. */
    class MetadataMapList :
        public musik::core::sdk::IMapList,
        public std::enable_shared_from_this<MetadataMapList>
    {
        public:
            /** @brief Creates an empty metadata map list. */
            MetadataMapList();
            virtual ~MetadataMapList();

            /* IMapList */
            /** @brief Frees the list (deletes this instance). */
            virtual void Release();
            /** @return The number of entries in the list. */
            virtual size_t Count() const;
            /** @return The entry at the given index, or nullptr.
             *  @param index Zero-based index. */
            virtual musik::core::sdk::IMap* GetAt(size_t index) const;

            /* implementation specific */
            /** @brief Removes all entries from the list. */
            void Clear();
            /** @brief Appends an entry to the list.
             *  @param entry The map to add. */
            void Add(MetadataMapPtr entry);

            /** @return A raw SDK IMapList pointer (borrowed). */
            musik::core::sdk::IMapList* GetSdkValue();
            /** @return The shared entry at the given index, or nullptr.
             *  @param index Zero-based index. */
            MetadataMapPtr GetSharedAt(size_t index) const;

        private:
            std::vector<MetadataMapPtr> entries; /**< Ordered list of entries. */
    };

    using MetadataMapListPtr = std::shared_ptr<MetadataMapList>; /**< Shared list alias. */

} }
