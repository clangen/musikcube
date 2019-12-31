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

#include <core/sdk/IBuffer.h>
#include <string.h>

template <typename T>
struct DataBuffer {
    DataBuffer() {
        data = nullptr;
        offset = rawLength = length = 0;
    }

    ~DataBuffer() {
        delete[] data;
    }

    void reset(size_t newLength) {
        if (newLength > rawLength) {
            delete[] data;
            data = new T[newLength];
        }
        rawLength = (newLength > rawLength) ? newLength : rawLength;
        length = newLength;
        offset = 0;
    }

    void reset() {
        offset = 0;
        length = 0;
    }

    void zero() {
        reset();
        if (data) {
            memset(data, 0, rawLength * sizeof(T));
        }
    }

    void from(T* source, size_t size) {
        reset(size);
        memcpy(this->data, source, size);
        length = size;
    }

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

    int append(const T* source, size_t size) {
        realloc(size);
        memcpy(data + length, source, size * sizeof(T));
        length += size;
        return (int) size;
    }

    int pad(char value, size_t size) {
        realloc(size);
        memset(data + length, value, size * sizeof(T));
        length += size;
        return size;
    }

    bool empty() {
        return offset >= length || length == 0;
    }

    size_t avail() {
        return (length > offset) ? length - offset : 0;
    }

    T* pos() {
        return data + offset;
    }

    void inc(size_t count) {
        offset += count;
        if (offset >= length) {
            offset = length = 0;
        }
    }

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

    T *data;
    size_t offset, length, rawLength;
};