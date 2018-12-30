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

#include "stdafx.h"
#include <mutex>

class RingBuffer {
    public:
        RingBuffer(int capacity) {
            this->capacity = capacity;
            this->data = new char[capacity];
            this->read = this->write = 0;
            this->absoluteOffset = 0;
        }

        ~RingBuffer() {
            delete[] data;
        }

        bool setReadOffset(unsigned int absolute) {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            if (absolute >= this->absoluteOffset && absolute <= this->absoluteOffset + size()) {
                read = (absolute - this->absoluteOffset);
                return true;
            }
            return false;
        }

        unsigned int size() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return write - read;
        }

        unsigned int avail() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return capacity - size();
        }

        bool empty() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return write == read;
        }

        bool full() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return size() == capacity;
        }

        void clear() {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            this->read = this->write = 0;
        }

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
        unsigned int mask(int val) {
            return val & (capacity - 1);
        }

        char* data;
        unsigned int capacity, read, write;
        unsigned int absoluteOffset;
        std::recursive_mutex mutex;
};