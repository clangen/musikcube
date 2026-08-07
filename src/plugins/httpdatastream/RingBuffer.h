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

/// @file RingBuffer.h
/// @brief Locked circular byte buffer used by the HTTP data stream plugin.
/// @details Provides a fixed-capacity FIFO for downloaded chunks. Indices grow
/// monotonically and are masked to the buffer size, so the ring supports
/// arbitrary sized put/get operations without explicit wraparound bookkeeping.

#include "config.h"
#include <mutex>

/** @brief Thread-safe fixed-capacity byte ring buffer.
 *  @details A producer calls put() to append bytes and a consumer calls get()
 *  to drain them. All operations are guarded by a recursive mutex. Read and
 *  write offsets are absolute values mapped into the buffer with a bit mask,
 *  which also allows setting the read offset relative to an absolute stream
 *  position. */
class RingBuffer {
    public:
        /** @brief Constructs a ring buffer of the given capacity.
         *  @param capacity Buffer size in bytes (should be a power of two). */
        RingBuffer(int capacity) {
            this->capacity = capacity;
            this->data = new char[capacity];
            this->read = this->write = 0;
            this->absoluteOffset = 0;
        }

        /** @brief Destroys the ring buffer and frees its storage. */
        ~RingBuffer() {
            delete[] data;
        }

        /** @brief Sets the read offset to an absolute stream position.
         *  @param absolute The absolute byte offset to read from.
         *  @return True if the offset falls within the buffered range. */
        bool setReadOffset(unsigned int absolute) {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            if (absolute >= this->absoluteOffset && absolute <= this->absoluteOffset + size()) {
                read = (absolute - this->absoluteOffset);
                return true;
            }
            return false;
        }

        /** @brief Returns the number of bytes currently buffered.
         *  @return Bytes available to read. */
        unsigned int size() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return write - read;
        }

        /** @brief Returns the free space in the buffer.
         *  @return Bytes that can still be written. */
        unsigned int avail() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return capacity - size();
        }

        /** @brief Returns whether the buffer is empty.
         *  @return True if no data is buffered. */
        bool empty() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return write == read;
        }

        /** @brief Returns whether the buffer is full.
         *  @return True if no space remains. */
        bool full() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return size() == capacity;
        }

        /** @brief Discards all buffered data. */
        void clear() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            this->read = this->write = 0;
        }

        /** @brief Appends bytes to the buffer.
         *  @param src Source bytes.
         *  @param len Number of bytes to append. */
        void put(char* src, unsigned int len) {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            int from = mask(write);
            int to = from + len;
            if (to > capacity) {
                int firstChunk = capacity - write;
                memcpy(&data[from], src, firstChunk);
                memcpy(&data[0], src + firstChunk, len - firstChunk);
            }
            else {
                memcpy(&data[from], src, len);
            }

            write += len;
        }

        /** @brief Removes up to len bytes from the buffer.
         *  @param dst Destination buffer.
         *  @param len Maximum number of bytes to read.
         *  @return Number of bytes actually read. */
        unsigned int get(char* dst, unsigned int len) {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            len = (size() < len ? size() : len);
            int from = mask(read);
            int to = from + len;
            if (to > capacity) {
                int firstChunk = capacity - read;
                memcpy(dst, &data[from], firstChunk);
                memcpy(dst + firstChunk, data, len - firstChunk);
            }
            else {
                memcpy(dst, data + from, len);
            }

            read += len;
            return len;
        }

    private:
        /** @brief Maps an absolute index into the buffer.
         *  @param val Absolute index to wrap.
         *  @return Index within the buffer storage. */
        unsigned int mask(int val) {
            return val & (capacity - 1);
        }

        /** @brief Raw buffer storage. */
        char* data;
        /** @brief Buffer capacity, read offset and write offset. */
        unsigned int capacity, read, write;
        /** @brief Absolute stream offset of the first buffered byte. */
        unsigned int absoluteOffset;
        /** @brief Guards all buffer operations. */
        std::recursive_mutex mutex;
};