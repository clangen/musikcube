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

#include <list>
#include <functional>
#include <memory>

namespace cursespp {
    template <typename T>
    class EntryPool: public std::enable_shared_from_this<EntryPool<T>> {
        public:
            EntryPool() {

            }

            ~EntryPool() {
                for (auto e : this->pool) {
                    delete e;
                }
            }

            std::shared_ptr<T> Get() {
                if (!deleter) {
                    std::weak_ptr<EntryPool<T>> weak = this->shared_from_this();
                    deleter = [weak](T* entry) {
                        if (!weak.expired()) {
                            weak.lock()->pool.push_front(entry);
                        }
                        else {
                            delete entry;
                        }
                    };
                }

                T* entry;
                if (pool.size()) {
                    entry = pool.front();
                    pool.pop_front();
                }
                else {
                    entry = new T();
                }

                return std::shared_ptr<T>(entry, deleter);
            }

        private:
            std::list<T*> pool;
            std::function<void(T*)> deleter;
    };
}