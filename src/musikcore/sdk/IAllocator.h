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

/** @file IAllocator.h @brief Defines the IAllocator interface and a helper for safe cross-module memory management. */
#pragma once

#include <stdlib.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /* plugins, while running in the same process space, may use different
    implementations of alloc() and dealloc(), so memory allocated by the main
    app should not be freed by a plugin, or vice versa. this simple interface
    allows plugins to pass an allocator to the main app so it can allocate and
    deallocate memory on the plugin's behalf. */
    /** @brief An abstract memory allocator that lets a plugin own and manage
     *  memory on behalf of the main application, avoiding cross-module
     *  allocation/deallocation mismatches. */
    class IAllocator {
        public:
            /** @brief Allocates a block of memory.
             *  @param size The number of bytes to allocate.
             *  @return A pointer to the newly allocated memory, or null on failure. */
            virtual void* Allocate(size_t size) = 0;

            /** @brief Frees a block of memory previously allocated by this allocator.
             *  @param data The pointer to free. */
            virtual void Free(void *) = 0;
    };

    /** @brief A concrete IAllocator backed by the standard C malloc/free functions. */
    template <typename T>
    class PluginAllocator: public IAllocator {
        public:
            /** @brief Allocates memory using malloc.
             *  @param size The number of bytes to allocate.
             *  @return A pointer to the allocated memory. */
            virtual void* Allocate(size_t size) { return malloc(size); }

            /** @brief Frees memory using free.
             *  @param data The pointer to free. */
            virtual void Free(void* data) { free(data); }
    };

} } }
