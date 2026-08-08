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

/** @file DataBuffer.h @brief Defines the DataBuffer template, a simple growable byte buffer utility. */
#pragma once

#include <musikcore/sdk/IBuffer.h>
#include <string.h>

/** @brief A simple, growable buffer for storing raw byte data with read/write tracking.
 *  @details Tracks a read offset and a write length over a heap-allocated
 *  array, and grows its underlying storage as needed when appending data.
 *  @tparam T The element type stored in the buffer. */
template <typename T>
struct DataBuffer {
    /** @brief Constructs an empty buffer with no allocated storage. */
    DataBuffer() {
        data = nullptr;
        offset = rawLength = length = 0;
    }

    /** @brief Frees the underlying storage. */
    ~DataBuffer() {
        delete[] data;
    }

    /** @brief Resizes the buffer to hold the given number of elements and resets the read offset.
     *  @param newLength The new capacity in elements. */
    void reset(size_t newLength) {
        if (newLength > rawLength) {
            delete[] data;
            data = new T[newLength];
        }
        rawLength = (newLength > rawLength) ? newLength : rawLength;
        length = newLength;
        offset = 0;
    }

    /** @brief Resets the read offset and logical length without resizing the storage. */
    void reset() {
        offset = 0;
        length = 0;
    }

    /** @brief Resets the buffer and zero-fills its raw storage.
     *  @details The logical length is cleared and the entire raw allocation is
     *  zeroed so no stale data remains. */
    void zero() {
        reset();
        if (data) {
            memset(data, 0, rawLength * sizeof(T));
        }
    }

    /** @brief Copies source data into the buffer, resizing it as necessary.
     *  @param source The source data to copy.
     *  @param size The number of elements to copy. */
    void from(T* source, size_t size) {
        reset(size);
        memcpy(this->data, source, size);
        length = size;
    }

    /** @brief Ensures the buffer has room for an additional number of elements.
     *  @details Doubles the raw capacity when the current storage is insufficient.
     *  @param size The number of additional elements to make room for. */
    void realloc(size_t size) {
        if (length + size > rawLength) {
            rawLength = (length + size) * 2;
            T* newData = new T[rawLength];
            if (length && data) {
                memcpy(newData, data, length * sizeof(T));
                delete[] data;
            }
            data = newData;
        }
    }

    /** @brief Appends source data to the end of the buffer.
     *  @param source The data to append.
     *  @param size The number of elements to append.
     *  @return The number of elements appended. */
    int append(const T* source, size_t size) {
        realloc(size);
        memcpy(data + length, source, size * sizeof(T));
        length += size;
        return (int) size;
    }

    /** @brief Appends a repeated byte value to the end of the buffer.
     *  @param value The byte value to append.
     *  @param size The number of elements to append.
     *  @return The number of elements appended. */
    int pad(char value, size_t size) {
        realloc(size);
        memset(data + length, value, size * sizeof(T));
        length += size;
        return size;
    }

    /** @brief Returns whether all data has been consumed.
     *  @return True when the read offset has reached the end of the data. */
    bool empty() {
        return offset >= length || length == 0;
    }

    /** @brief Returns the number of unconsumed elements available from the current read offset.
     *  @return The remaining element count. */
    size_t avail() {
        return (length > offset) ? length - offset : 0;
    }

    /** @brief Returns a pointer to the current read position.
     *  @return A pointer into the buffer at the read offset. */
    T* pos() {
        return data + offset;
    }

    /** @brief Advances the read offset by the given number of elements.
     *  @details When the offset reaches the end of the data, both the offset and
     *  the logical length are reset to zero.
     *  @param count The number of elements to advance. */
    void inc(size_t count) {
        offset += count;
        if (offset >= length) {
            offset = length = 0;
        }
    }

    /** @brief Swaps the internal state of this buffer with another buffer.
     *  @param with The buffer whose state will be exchanged with this one. */
    void swap(DataBuffer& with) {
        size_t off = offset, len = length, raw = rawLength;
        T* d = data;
        this->data = with.data;
        this->length = with.length;
        this->rawLength = with.rawLength;
        this->offset = with.offset;
        with.data = d;
        with.length = len;
        with.rawLength = raw;
        with.offset = offset;
    }

    /** @brief Pointer to the underlying heap-allocated storage. */
    T *data;
    /** @brief The current read offset, the logical length of valid data, and
     *  the raw allocated capacity, each in elements. */
    size_t offset, length, rawLength;
};